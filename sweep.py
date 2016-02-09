#!/usr/bin/env python3

import argparse
import multiprocessing
import os
import re
import shutil
import sys
import taskrun


def getName(filename):
  return os.path.splitext(os.path.basename(filename))[0]


def getFiles(filename, run, idx):
  name = getName(filename)
  names = {
    'name': name,
    'json': filename,
    'log' : os.path.join(run, '{0}_{1}.log.gz'.format(name, idx)),
    'plot': os.path.join(run, '{0}_{1}.png'.format(name, idx)),
    'data': os.path.join(run, '{0}_{1}.txt'.format(name, idx))}
  return names


def main(args):
  # create the task manager
  rm = taskrun.ResourceManager(taskrun.CounterResource('core', 1, args.cores))
  tm = taskrun.TaskManager(
    resource_manager=rm,
    observer=taskrun.VerboseObserver(),
    failure_mode=taskrun.FailureMode.ACTIVE_CONTINUE)

  # create all tasks
  for f in os.listdir('json'):
    f = os.path.join('json', f)
    if os.path.isfile(f) and f.endswith('.json'):
      if re.search(args.regex, getName(f)):
        for val in args.vals:
          io = getFiles(f, args.run, val)
          sim_cmd = 'bin/ratesim {0} log_file=string={1}'.format(
            io['json'], io['log'])
          sim_cmd += ' {0}{1}'.format(args.mod, val)
          if args.override:
            for override in args.override:
              sim_cmd += ' {0}'.format(override)
          sim = taskrun.ProcessTask(
            tm, 'sim_{0}'.format(io['name']), sim_cmd)
          plot_cmd = 'parser/run.py {0}'.format(io['log'])
          if args.plot:
            plot_cmd += ' -s {0} -p {1}'.format(args.smoothing, io['plot'])
          if args.data:
            plot_cmd += ' -d {0}'.format(io['data'])
          plot = taskrun.ProcessTask(tm, 'plot_{0}'.format(io['name']), plot_cmd)
          plot.add_dependency(sim)

  # set up output directory
  if os.path.isdir(args.run):
    if args.force:
      shutil.rmtree(args.run)
    else:
      while True:
        ans = input('\'{0}\' already exists, remove it? (N/y) '
                    .format(args.run)).lower()
        if ans == '' or ans == 'no' or ans == 'n':
          return -1;
        elif ans == 'yes' or ans == 'y':
          shutil.rmtree(args.run)
          break
  os.mkdir(args.run)

  # run all tasks
  tm.run_tasks()


if __name__ == '__main__':
  ap = argparse.ArgumentParser()
  ap.add_argument('-f', '--force', action='store_true',
                  help='remove old files forcefully')
  ap.add_argument('run',
                  help='name of the run (directory)')
  ap.add_argument('mod',
                  help='the command line modifier without value')
  ap.add_argument('vals', nargs='+',
                  help='the values for the sweep')
  ap.add_argument('-s', '--smoothing', type=int, default=50,
                  help='plot line smoothing factor')
  ap.add_argument('-r', '--regex', default='.*',
                  help='regex to match names')
  ap.add_argument('-o', '--override', nargs='*',
                  help='override string for settings files')
  ap.add_argument('-c', '--cores', type=int,
                  default=multiprocessing.cpu_count(),
                  help='number of cores to use during run')
  ap.add_argument('-p', '--plot', action='store_true',
                  help='generate plot files')
  ap.add_argument('-d', '--data', action='store_true',
                  help='generate data files')
  args = ap.parse_args()
  sys.exit(main(args))
