import os
import argparse
import logging
import sys
import traceback
import shlex
import datetime
from inspect import signature
from collections import OrderedDict
from multiprocessing import cpu_count
from .util import FatalError, Namespace, qjoin
from .instance import Instance
from .target import Target
from .parallel import ProcessPool, PrunPool


# disable .pyc file generation
sys.dont_write_bytecode = True


class Setup:
    """
    Defines framework commands.

    The setup takes care of complicated things like command-line parsing,
    logging, parallelism, environment setup and generating build paths. You
    should only need to use the methods documented here. To use the setup, you
    must first populate it with targets and instances using :func:`add_target`
    and :func:`add_instance`, and then call :func:`main` to run the command
    issued in the command-line arguments:

    .. _setup-example:

    ::

        setup = infra.Setup(__file__)
        setup.add_instance(MyAwesomeInstance())
        setup.add_target(MyBeautifulTarget())
        setup.main()

    :func:`main` creates a :py:attr:`configuration context<ctx>` that it passes
    to methods of targets/instances/packages. You can see it being used as
    ``ctx`` by many API methods below. The context contains setup configuration
    data, such as absolute build paths, and environment variables for build/run
    commands, such as which compiler and CFLAGS to use to build the current
    target. Your own targets and instances should read/write to the context.

    **The job of an instance is to manipulate the the context such that a
    target is built in the desired way.** This manipulation happens in
    predefined API methods which you must overwrite (see below). Hence, these
    methods receive the context as a parameter.
    """

    ctx = None
    """
    :class:`util.Namespace` The configuration context.

    Consider an example project hosted in directory `/project`, with the
    infrastructure cloned as a submodule in `/project/infra` and a setup script
    like the one :ref:`above <setup-example>` in `/project/setup.py`. The context
    will look like this after initialization::

        Namespace({
            'log':      logging.Logger(...),
            'args':     argparse.Namespace(...),
            'jobs':     8,
            'paths':    Namespace({
                            'root':         '/project',
                            'setup':        '/project/setup.py',
                            'infra':        '/project/infra'
                            'buildroot':    '/project/build',
                            'log':          '/project/build/log',
                            'debuglog':     '/project/build/log/debug.txt',
                            'runlog':       '/project/build/log/commands.txt',
                            'packages':     '/project/build/packages',
                            'targets':      '/project/build/targets',
                            'pool_results': '/project/results'
                        }),
            'runenv':   Namespace({}),
            'cc':       'cc',
            'cxx':      'c++',
            'ar':       'ar',
            'nm':       'nm',
            'ranlib':   'ranlib',
            'cflags':   [],
            'cxxflags': [],
            'ldflags':  [],
            'hooks':    Namespace({
                            'post_build': []
                        }),
            'datetime': datetime.datetime
        })

    The :class:`util.Namespace` class is simply a :class:`dict` whose members
    can be accessed like attributes.

    ``ctx.log`` is a logging object used for status updates. Use this to
    provide useful information about what your implementation is doing.

    ``ctx.args`` is populated with processed command-line arguments, It is
    available to read custom build/run arguments from that are added by
    targets/instances.

    ``ctx.jobs`` contains the value of the ``-j`` command-line option,
    defaulting to the number of CPU cores returned by
    :func:`multiprocessing.cpu_count`.

    ``ctx.paths`` are absolute paths to be used (readonly) throughout the
    framework.

    ``ctx.runenv`` defines environment variables for :func:`util.run`, which is
    a wrapper for :func:`subprocess.run` that does logging and other useful
    things. 

    ``ctx.{cc,cxx,ar,nm,ranlib}`` define default tools of the compiler
    toolchain, and should be used by target definitions to configure build
    scripts. ``ctx.{c,cxx,ld}flags`` similarly define build flags for targets
    in a list and should be joined into a string using :func:`util.qjoin` when
    being passed as a string to a build script by a target definition.

    ``ctx.hooks.post_build`` defines a list of post-build hooks, which are
    python functions called with the path to the binary as the only parameter.

    ``ctx.starttime`` is set to ``datetime.datetime.now()``.

    ``ctx.workdir`` is set to the work directory from which the setup script is
    invoked.
    """

    _max_default_jobs = 16

    def __init__(self, setup_path: str):
        """
        :param setup_path: Path to the script running :func:`Setup.main`.
                           Needed to allow build scripts to call back into the
                           setup script for build hooks.
        """
        self.setup_path = os.path.abspath(setup_path)
        self.instances = OrderedDict()
        self.targets = OrderedDict()

    def _parse_argv(self):
        parser = argparse.ArgumentParser(
                description='Frontend for building/running instrumented benchmarks.')

        ncpus = cpu_count()
        nproc = min(ncpus, self._max_default_jobs)
        proc_default_parallelmax = nproc
        prun_default_parallelmax = 64

        targets_help = '%s' % ' | '.join(self.targets)
        instances_help = '%s' % ' | '.join(self.instances)

        # global options
        parser.add_argument('-v', '--verbosity', default='info',
                choices=['critical', 'error', 'warning', 'info', 'debug'],
                help='set logging verbosity (default info)')

        self.subparsers = parser.add_subparsers(
                title='subcommands', metavar='COMMAND', dest='command',
                description='run with "<command> --help" to see options for '
                            'individual commands')
        self.subparsers.required = True

        # command: build
        pbuild = self.subparsers.add_parser('build',
                help='build target programs (also builds dependencies)')
        pbuild.add_argument('-t', '--targets', nargs='+',
                metavar='TARGET', choices=self.targets, default=[],
                help=targets_help)
        pbuild.add_argument('-i', '--instances', nargs='+',
                metavar='INSTANCE', choices=self.instances, default=[],
                help=instances_help)
        pbuild.add_argument('-p', '--packages', nargs='+',
                metavar='PACKAGE', default=[],
                help='which packages to build (either on top of dependencies, '
                     'or to force a rebuild)').completer = self._complete_pkg
        pbuild.add_argument('-j', '--jobs', type=int, default=nproc,
                help='maximum number of build processes (default %d)' % nproc)
        pbuild.add_argument('--deps-only', action='store_true',
                help='only build dependencies, not targets themselves')
        pbuild.add_argument('--force-rebuild-deps', action='store_true',
                help='always run the build commands')
        pbuild.add_argument('--relink', action='store_true',
                help='only link targets, don\'t rebuild object files')
        pbuild.add_argument('--clean', action='store_true',
                help='clean targets and packages (not all deps, only from -p) first')
        pbuild.add_argument('--dry-run', action='store_true',
                help='don\'t actually build anything, just show what will be done')
        pbuild.add_argument('--parallel', choices=('proc', 'prun'), default=None,
                help='build benchmarks in parallel ("proc" for local processes, "prun" for DAS cluster)')
        pbuild.add_argument('--parallelmax', metavar='PROCESSES_OR_NODES', type=int,
                help='limit simultaneous node reservations (default: %d for proc, %d for prun)' %
                     (proc_default_parallelmax, prun_default_parallelmax))
        pbuild.add_argument('--prun-opts', default='',
                help='additional options for prun (for --parallel=prun)')

        # command: exec-hook
        # this does not appear in main --help usage because it is meant to be
        # used as a callback for build scripts
        phook = self.subparsers.add_parser('exec-hook')
        phook.add_argument('hooktype', choices=['post-build'],
                help='hook type')
        phook.add_argument('instance',
                metavar='INSTANCE', choices=self.instances,
                help=instances_help)
        phook.add_argument('targetfile', metavar='TARGETFILE',
                help='file to run hook on')

        # command: clean
        pclean = self.subparsers.add_parser('clean',
                help='remove all source/build/install files of the given '
                     'packages and targets')
        pclean.add_argument('-t', '--targets', nargs='+', metavar='TARGET',
                default=[], choices=self.targets,
                help=targets_help)
        pclean.add_argument('-p', '--packages', nargs='+', metavar='PACKAGE',
                default=[],
                help='which packages to clean').completer = self._complete_pkg

        # command: run
        prun = self.subparsers.add_parser('run',
                help='run a single target program')
        prun.add_argument('--build', action='store_true',
                help='build target first (default false)')
        prun.add_argument('-j', '--jobs', type=int, default=nproc,
                help='maximum number of build processes (default %d)' % nproc)
        prun.add_argument('-n', '--iterations', metavar='N',
                type=int, default=1,
                help='number of runs per benchmark')
        prun.add_argument('--parallel', choices=('proc', 'prun'), default=None,
                help='run benchmarks in parallel ("proc" for local processes, "prun" for DAS cluster)')
        prun.add_argument('--parallelmax', metavar='PROCESSES_OR_NODES', type=int,
                help='limit simultaneous node reservations (default: %d for proc, %d for prun)' %
                     (proc_default_parallelmax, prun_default_parallelmax))
        prun.add_argument('--prun-opts', default='',
                help='additional options for prun (for --parallel=prun)')
        prun_targets = prun.add_subparsers(
                title='target', metavar='TARGET', dest='target',
                help=targets_help)
        prun_targets.required = True

        # command: report
        preport = self.subparsers.add_parser('report',
                help='report results after a run')
        preport.add_argument('-i', '--instances', nargs='+', metavar='INSTANCE',
                default=[], choices=self.instances,
                help=instances_help)
        preport.add_argument('-o', '--outfile', type=argparse.FileType('w'),
                default=sys.stdout,
                help='outfile (default: stdout)')
        preport_targets = preport.add_subparsers(
                title='target', metavar='TARGET', dest='target',
                help=targets_help)
        preport_targets.required = True

        # command: config
        pconfig = self.subparsers.add_parser('config',
                help='print information about command line arguments and build flags')
        pconfig_group = pconfig.add_mutually_exclusive_group(required=True)
        pconfig_group.add_argument('--instances', action='store_true',
                dest='list_instances',
                help='list all registered instances')
        pconfig_group.add_argument('--targets', action='store_true',
                dest='list_targets',
                help='list all registered targets')
        pconfig_group.add_argument('--packages', action='store_true',
                dest='list_packages',
                help='list dependencies of all registered targets/instances')

        # command: pkg-config
        # TODO: one subparser per package (is less efficient though)
        ppkgconfig = self.subparsers.add_parser('pkg-config',
                help='print package-specific information')
        ppkgconfig.add_argument('package',
                help='package to configure').completer = self._complete_pkg
        #ppkgconfig.add_argument('option',
        #        help='configuration option').completer = self.complete_pkg_config
        ppkgconfig.add_argument('args', nargs=argparse.REMAINDER, choices=[],
                help='configuration args (package dependent)')

        for target in self.targets.values():
            target.add_build_args(pbuild)

            ptarget = prun_targets.add_parser(target.name)
            ptarget.add_argument('instances', nargs='+',
                    metavar='INSTANCE', choices=self.instances,
                    help=instances_help)
            target.add_run_args(ptarget)

            ptarget = preport_targets.add_parser(target.name)
            target.add_report_args(ptarget)

        for instance in self.instances.values():
            instance.add_build_args(pbuild)

        # enable bash autocompletion if supported
        try:
            import argcomplete

            # use a custom completer that moves non-positional options to the
            # end of the completion list, and excludes --help
            class MyCompleter(argcomplete.CompletionFinder):
                def filter_completions(self, completions):
                    completions = super().filter_completions(completions)
                    if completions:
                        for i, value in enumerate(completions):
                            if not value.startswith('-'):
                                return completions[i:] + completions[:i]
                    return completions

            MyCompleter().__call__(parser, exclude=['--help', 'exec-hook'])
        except ImportError:
            pass

        self.ctx.args = self.args = parser.parse_args()

        if 'jobs' in self.args:
            self.ctx.jobs = self.args.jobs

        if 'prun_opts' in self.args:
            self.args.prun_opts = shlex.split(self.args.prun_opts)

        if 'parallelmax' in self.args and self.args.parallelmax is None:
            if self.args.parallel == 'proc':
                self.args.parallelmax = proc_default_parallelmax
            elif self.args.parallel == 'prun':
                self.args.parallelmax = prun_default_parallelmax

    def _complete_pkg(self, prefix, parsed_args, **kwargs):
        objs = list(self.targets.values())
        objs += list(self.instances.values())
        for package in self._get_deps(objs):
            name = package.ident()
            if name.startswith(prefix):
                yield name

    def _init_context(self):
        self.ctx.hooks = Namespace(post_build=[])

        self.ctx.paths = paths = Namespace()
        paths.setup = self.setup_path
        paths.root = os.path.dirname(self.setup_path)
        paths.infra = os.path.dirname(os.path.dirname(__file__))
        paths.buildroot = os.path.join(paths.root, 'build')
        paths.log = os.path.join(paths.buildroot, 'log')
        paths.debuglog = os.path.join(paths.log, 'debug.txt')
        paths.runlog = os.path.join(paths.log, 'commands.txt')
        paths.packages = os.path.join(paths.buildroot, 'packages')
        paths.targets = os.path.join(paths.buildroot, 'targets')
        paths.pool_results = os.path.join(paths.root, 'results')

        self.ctx.runenv = Namespace()
        self.ctx.cc = 'cc'
        self.ctx.cxx = 'c++'
        self.ctx.ar = 'ar'
        self.ctx.nm = 'nm'
        self.ctx.ranlib = 'ranlib'
        self.ctx.cflags = []
        self.ctx.cxxflags = []
        self.ctx.ldflags = []

        self.ctx.starttime = datetime.datetime.now()
        self.ctx.workdir = os.getcwd()

    def _create_dirs(self):
        os.makedirs(self.ctx.paths.log, exist_ok=True)
        os.makedirs(self.ctx.paths.packages, exist_ok=True)
        os.makedirs(self.ctx.paths.targets, exist_ok=True)

    def _initialize_logger(self):
        fmt = '%(asctime)s [%(levelname)s] %(message)s'
        datefmt = '%H:%M:%S'

        self.ctx.log = log = logging.getLogger('autosetup')
        log.setLevel(logging.DEBUG)
        log.propagate = False
        self.ctx.loglevel = getattr(logging, self.args.verbosity.upper())

        termlog = logging.StreamHandler(sys.stdout)
        termlog.setLevel(self.ctx.loglevel)
        termlog.setFormatter(logging.Formatter(fmt, datefmt))
        log.addHandler(termlog)

        # always write debug log to file
        debuglog = logging.FileHandler(self.ctx.paths.debuglog, mode='w')
        debuglog.setLevel(logging.DEBUG)
        debuglog.setFormatter(logging.Formatter(fmt, '%Y-%m-%d ' + datefmt))
        log.addHandler(debuglog)

        # colorize log if supported
        try:
            import coloredlogs
            coloredlogs.install(logger=log, fmt=fmt, datefmt=datefmt,
                                level=termlog.level)
        except ImportError:
            pass

    def add_instance(self, instance: Instance):
        """
        Register an instance. Only registered instances can be referenced in
        commands, so also :doc:`built-in instances <instances>` must be
        registered.

        :param instance: The instance to register.
        """
        if instance.name in self.instances:
            raise FatalError('overwriting existing instance "%s"' %
                             instance.name)
        self.instances[instance.name] = instance

    def _get_instance(self, name):
        if name not in self.instances:
            raise FatalError('no instance called "%s"' % name)
        return self.instances[name]

    def add_target(self, target: Target):
        """
        Register a target. Only registered targets can be referenced in
        commands, so also :doc:`built-in targets <targets>` must be registered.

        :param target: The target to register.
        """
        if target.name in self.targets:
            raise FatalError('overwriting existing target "%s"' % target.name)
        self.targets[target.name] = target

    def _get_target(self, name):
        if name not in self.targets:
            raise FatalError('no target called "%s"' % name)
        return self.targets[name]

    def _get_package(self, name):
        objs = list(self.targets.values())
        objs += list(self.instances.values())
        for package in self._get_deps(objs):
            if package.ident() == name:
                return package
        raise FatalError('no package called %s' % name)

    def _get_deps(self, objs):
        deps = []

        def add_dep(dep, visited):
            if dep in visited:
                raise FatalError('recursive dependency %s' % dep)
            visited.add(dep)

            for nested_dep in dep.dependencies():
                add_dep(nested_dep, set(visited))

            #if dep in deps:
            #    self.ctx.log.debug('skipping duplicate dependency %s' % dep.ident())
            #else:
            if dep not in deps:
                deps.append(dep)

        for obj in objs:
            for dep in obj.dependencies():
                add_dep(dep, set())

        return deps

    def _fetch_package(self, package, force_rebuild, *args):
        package.goto_rootdir(self.ctx)

        if package.is_fetched(self.ctx):
            self.ctx.log.debug('%s already fetched, skip' % package.ident())
        elif not force_rebuild and package.is_installed(self.ctx):
            self.ctx.log.debug('%s already installed, skip fetching' % package.ident())
        else:
            self.ctx.log.info('fetching %s' % package.ident())
            if not self.args.dry_run:
                package.goto_rootdir(self.ctx)
                package.fetch(self.ctx, *args)

    def _build_package(self, package, force_rebuild, *args):
        package.goto_rootdir(self.ctx)

        if not force_rebuild and package.is_built(self.ctx):
            self.ctx.log.debug('%s already built, skip' % package.ident())
            return
        elif not force_rebuild and package.is_installed(self.ctx):
            self.ctx.log.debug('%s already installed, skip building' % package.ident())
        else:
            self.ctx.log.info('building %s' % package.ident())
            if not self.args.dry_run:
                package.goto_rootdir(self.ctx)
                package.build(self.ctx, *args)

    def _install_package(self, package, force_rebuild, *args):
        package.goto_rootdir(self.ctx)

        if not force_rebuild and package.is_installed(self.ctx):
            self.ctx.log.debug('%s already installed, skip' % package.ident())
        else:
            self.ctx.log.info('installing %s' % package.ident())
            if not self.args.dry_run:
                package.goto_rootdir(self.ctx)
                package.install(self.ctx, *args)

        package.goto_rootdir(self.ctx)

    def _load_deps(self, obj):
        for package in self._get_deps([obj]):
            self.ctx.log.debug('install %s into env' % package.ident())
            package.install_env(self.ctx)

    def _clean_package(self, package):
        if package.is_clean(self.ctx):
            self.ctx.log.debug('package %s is already cleaned' % package.ident())
        else:
            self.ctx.log.info('cleaning package ' + package.ident())
            if not self.args.dry_run:
                package.clean(self.ctx)

    def _clean_target(self, target):
        if target.is_clean(self.ctx):
            self.ctx.log.debug('target %s is already cleaned' % target.name)
        else:
            self.ctx.log.info('cleaning target ' + target.name)
            if not self.args.dry_run:
                target.clean(self.ctx)

    def _make_pool(self):
        if self.args.parallel == 'proc':
            if len(self.args.prun_opts):
                raise FatalError('--prun-opts not supported for --parallel=proc')
            return ProcessPool(self.ctx.log, self.args.parallelmax)

        if self.args.parallel == 'prun':
            return PrunPool(self.ctx.log, self.args.parallelmax,
                            self.args.prun_opts)

        if self.args.parallelmax:
            raise FatalError('--parallelmax not supported for --parallel=none')
        if len(self.args.prun_opts):
            raise FatalError('--prun-opts not supported for --parallel=none')

    def _run_build(self):
        targets = [self._get_target(name) for name in self.args.targets]
        instances = [self._get_instance(name) for name in self.args.instances]
        packages = [self._get_package(name) for name in self.args.packages]
        pool = self._make_pool()

        if self.args.deps_only:
            if not targets and not instances and not packages:
                raise FatalError('no targets or instances specified')
        elif (not targets or not instances) and not packages:
            raise FatalError('need at least one target and instance to build')

        deps = self._get_deps(targets + instances + packages)
        force_deps = set()
        separate_packages = []
        for package in packages:
            if package in deps:
                force_deps.add(package)
            else:
                separate_packages.append(package)

        # clean packages and targets if requested
        if self.args.clean:
            for package in packages:
                self._clean_package(package)
            for target in targets:
                self._clean_target(target)

        # first fetch all necessary code so that the internet connection can be
        # broken during building
        for package in deps:
            self._fetch_package(package, self.args.force_rebuild_deps)
        for package in separate_packages:
            self._fetch_package(package, True)

        if not self.args.deps_only:
            for target in targets:
                target.goto_rootdir(self.ctx)
                if target.is_fetched(self.ctx):
                    self.ctx.log.debug('%s already fetched, skip' % target.name)
                else:
                    self.ctx.log.info('fetching %s' % target.name)
                    target.fetch(self.ctx)

        cached_deps = {t: self._get_deps([t]) for t in targets}
        for i in instances:
            cached_deps[i] = self._get_deps([i])
        for p in separate_packages:
            cached_deps[p] = self._get_deps([p])

        built_packages = set()

        def build_package_once(package, force):
            if package not in built_packages:
                self._build_package(package, force)
                self._install_package(package, force)
                built_packages.add(package)

        def build_deps_once(obj):
            for package in cached_deps[obj]:
                force = self.args.force_rebuild_deps or package in force_deps
                build_package_once(package, force)
                self.ctx.log.debug('install %s in env' % package.ident())
                package.install_env(self.ctx)

        for package in separate_packages:
            oldctx = self.ctx.copy()
            build_deps_once(package)
            build_package_once(package, True)
            self.ctx = oldctx

        if self.args.deps_only and not instances:
            for target in targets:
                oldctx = self.ctx.copy()
                build_deps_once(target)
                self.ctx = oldctx

        for instance in instances:
            # use a copy of the context for instance configuration to avoid
            # stacking configurations between instances
            # FIXME: only copy the build env (the part that changes)
            oldctx_outer = self.ctx.copy()
            instance.configure(self.ctx)
            build_deps_once(instance)

            for target in targets:
                oldctx_inner = self.ctx.copy()
                build_deps_once(target)

                if not self.args.deps_only:
                    self.ctx.log.info('building %s-%s' % (target.name, instance.name))
                    if not self.args.dry_run:
                        if not self.args.relink:
                            target.goto_rootdir(self.ctx)
                            self._call_with_pool(target.build,
                                                 (self.ctx, instance), pool)
                        target.goto_rootdir(self.ctx)
                        self._call_with_pool(target.link, (self.ctx, instance), pool)
                        target.run_hooks_post_build(self.ctx, instance)

                self.ctx = oldctx_inner

            self.ctx = oldctx_outer

        if pool:
            pool.wait_all()

    def _call_with_pool(self, fn, args, pool):
        if len(signature(fn).parameters) == len(args) + 1:
            fn(*args, pool)
            return True
        if pool:
            return False
        fn(*args)
        return True

    def _run_exec_hook(self):
        instance = self._get_instance(self.args.instance)

        absfile = os.path.abspath(self.args.targetfile)
        if not os.path.exists(absfile):
            raise FatalError('file %s does not exist' % absfile)

        hooktype = self.args.hooktype.replace('-', '_')
        assert hooktype in self.ctx.hooks

        # don't build packages (should have been done already since this
        # command should only be called recursively), just load them
        self._load_deps(instance)

        # populate self.ctx.hooks[hooktype]
        instance.configure(self.ctx)

        # run hooks
        basedir = os.path.dirname(absfile)
        for hook in self.ctx.hooks[hooktype]:
            os.chdir(basedir)
            hook(self.ctx, absfile)

    def _run_clean(self):
        packages = [self._get_package(name) for name in self.args.packages]
        targets = [self._get_target(name) for name in self.args.targets]
        if not packages and not targets:
            raise FatalError('no packages or targets specified')

        self.args.dry_run = False
        for package in packages:
            self._clean_package(package)
        for target in targets:
            self._clean_target(target)

    def _run_run(self):
        target = self._get_target(self.args.target)
        instances = [self._get_instance(name) for name in self.args.instances]
        pool = self._make_pool()

        if self.args.build:
            self.args.targets = [self.args.target]
            self.args.packages = []
            self.args.deps_only = False
            self.args.clean = False
            self.args.force_rebuild_deps = False
            self.args.dry_run = False
            self.args.relink = False
            self.ctx.jobs = min(cpu_count(), self._max_default_jobs)
            self._run_build()

        self._load_deps(target)

        for instance in instances:
            oldctx = self.ctx.copy()
            self.ctx.log.info('running %s-%s' % (target.name, instance.name))

            self._load_deps(instance)
            instance.prepare_run(self.ctx)
            target.goto_rootdir(self.ctx)

            if not self._call_with_pool(target.run, (self.ctx, instance), pool):
                raise FatalError('target %s does not support parallel runs' %
                                 target.name)

            self.ctx = oldctx

        if pool:
            pool.wait_all()

    def _run_report(self):
        target = self._get_target(self.args.target)
        instances = [self._get_instance(name) for name in self.args.instances]
        target.report(self.ctx, instances, self.args.outfile, self.args)

    def _run_config(self):
        if self.args.list_instances:
            for name in self.instances.keys():
                print(name)
        elif self.args.list_targets:
            for name in self.targets.keys():
                print(name)
        elif self.args.list_packages:
            objs = list(self.targets.values())
            objs += list(self.instances.values())
            for package in self._get_deps(objs):
                print(package.ident())
        else:
            raise NotImplementedError

    def _run_pkg_config(self):
        package = self._get_package(self.args.package)
        parser = self.subparsers.add_parser(
                '%s %s' % (self.args.command, package.ident()))
        pgroup = parser.add_mutually_exclusive_group(required=True)
        for opt, desc, value in package.pkg_config_options(self.ctx):
            pgroup.add_argument(opt, action='store_const', dest='value',
                                const=value, help=desc)
        value = parser.parse_args(self.args.args).value

        # for lists (handy for flags), join by spaces while adding quotes where
        # necessary
        if isinstance(value, (list, tuple)):
            value = qjoin(value)

        print(value)

    def _run_command(self):
        try:
            if self.args.command not in ('exec-hook', 'config', 'pkg-config'):
                os.chdir(self.ctx.paths.root)
                self.ctx.runlog = open(self.ctx.paths.runlog, 'w')

            if self.args.command == 'build':
                self._run_build()
            elif self.args.command == 'exec-hook':
                self._run_exec_hook()
            elif self.args.command == 'clean':
                self._run_clean()
            elif self.args.command == 'run':
                self._run_run()
            elif self.args.command == 'report':
                self._run_report()
            elif self.args.command == 'config':
                self._run_config()
            elif self.args.command == 'pkg-config':
                self._run_pkg_config()
            else:
                raise FatalError('unknown command %s' % self.args.command)
        except FatalError as e:
            self.ctx.log.error(str(e))
        except KeyboardInterrupt:
            self.ctx.log.warning('exiting because of keyboard interrupt')
        except Exception as e:
            self.ctx.log.critical('unkown error\n' + traceback.format_exc().rstrip())

    def main(self):
        """
        Run the configured setup:

        #. Parse command-line arguments.
        #. Create build directories and log files.
        #. Run the issued command.
        """
        self.ctx = Namespace()
        self._init_context()
        self._parse_argv()
        self._create_dirs()
        self._initialize_logger()
        self._run_command()
