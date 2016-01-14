#!/usr/bin/env python3

import argparse
import os
import taskrun


def names(filename):
  name = os.path.splitext(os.path.basename(filename))[0]
  names = {
    'name': name,
    'json': filename,
    'log': '{0}.log.gz'.format(name),
    'plot': '{0}.png'.format(name)}
  return names


def main(args):
  # create all tasks
  tm = taskrun.TaskManager(
    observer=taskrun.VerboseObserver(),
    failure_mode=taskrun.FailureMode.ACTIVE_CONTINUE)
  for f in os.listdir('json'):
    f = os.path.join('json', f)
    if os.path.isfile(f) and f.endswith('.json'):
      io = names(f)
      sim = taskrun.ProcessTask(
        tm, 'sim_{0}'.format(io['name']),
        'bin/ratesim {0} log_file=string={1}'.format(
          io['json'], io['log']))
      plot = taskrun.ProcessTask(
        tm, 'plot_{0}'.format(io['name']),
        'parser/run.py {0} -p {1} -s {2}'.format(
          io['log'], io['plot'], args.smoothing))
      plot.add_dependency(sim)

  # run all tasks
  tm.run_tasks()


if __name__ == '__main__':
  ap = argparse.ArgumentParser()
  ap.add_argument('-f', '--force', action='store_true',
                  help='remove old files forcefully')
  ap.add_argument('-s', '--smoothing', type=int, default=0,
                  help='plot line smoothing factor')
  args = ap.parse_args()
  main(args)
