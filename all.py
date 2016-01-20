#!/usr/bin/env python3

import argparse
import os
import re
import shutil
import sys
import taskrun


def names(filename, run):
  name = os.path.splitext(os.path.basename(filename))[0]
  names = {
    'name': name,
    'json': filename,
    'log' : os.path.join(run, '{0}.log.gz'.format(name)),
    'plot': os.path.join(run, '{0}.png'.format(name))}
  return names


def main(args):
  # create all tasks
  tm = taskrun.TaskManager(
    observer=taskrun.VerboseObserver(),
    failure_mode=taskrun.FailureMode.ACTIVE_CONTINUE)
  for f in os.listdir('json'):
    f = os.path.join('json', f)
    if os.path.isfile(f) and f.endswith('.json'):
      io = names(f, args.run)
      if re.search(args.names, io['name']):
        sim_cmd = 'bin/ratesim {0} log_file=string={1}'.format(
          io['json'], io['log'])
        for override in args.override:
          sim_cmd += ' {0}'.format(override)
        sim = taskrun.ProcessTask(
          tm, 'sim_{0}'.format(io['name']), sim_cmd)
        plot = taskrun.ProcessTask(
          tm, 'plot_{0}'.format(io['name']),
          'parser/run.py {0} -p {1} -s {2}'.format(
            io['log'], io['plot'], args.smoothing))
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
  ap.add_argument('-s', '--smoothing', type=int, default=50,
                  help='plot line smoothing factor')
  ap.add_argument('-n', '--names', default='.*',
                  help='regex to match names')
  ap.add_argument('-o', '--override', nargs='*',
                  help='override string for settings files')
  args = ap.parse_args()
  sys.exit(main(args))
