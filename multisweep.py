#!/usr/bin/env python3

import argparse
import math
import os
import psutil
import re
import shutil
import sys
import taskrun


def getName(filename):
  return os.path.splitext(os.path.basename(filename))[0]


def getFiles(filename, batch, config, code):
  name = getName(filename)
  id = '{0}_{1}'.format(config, code)
  names = {
    'json': filename,
    'id': id,
    'log' : os.path.join(batch, '{0}.log.gz'.format(id)),
    'plot': os.path.join(batch, '{0}.png'.format(id)),
    'data': os.path.join(batch, '{0}.txt'.format(id))}
  return names


def totalCores():
  return os.cpu_count()


def availableMemory(factor=1.0):
  psmem = psutil.virtual_memory()
  total = math.floor((psmem.free + psmem.cached) / (1024 * 1024 * 1024))
  assert factor >= 0.0 and factor <= 1.0
  return total * factor


def rawSettings():
  return [
    {
      'code': 'mt',
      'path': 'sender_config.params.max_tokens',
      'type': 'uint',
      'values': ['1000'], #'1500'], #'2000', '2500', '3000'],
      'affects': ['dist1', 'dist2', 'dist3', 'dist4']
    },
    {
      'code': 'st',
      'path': 'sender_config.params.steal_threshold',
      'type': 'float',
      'values': ['0.10', '0.20', '0.30'],
      'affects': ['dist2', 'dist3', 'dist4']
    },
    {
      'code': 'tak',
      'path': 'sender_config.params.token_ask_factor',
      'type': 'float',
      'values': ['0.80', '1.00', '1.20'],
      'affects': ['dist2', 'dist4']
    },
    {
      'code': 'rak',
      'path': 'sender_config.params.rate_ask_factor',
      'type': 'float',
      'values': ['0.90', '1.00'],
      'affects': ['dist3', 'dist4']
    },
    {
      'code': 'mro',
      'path': 'sender_config.params.max_requests_outstanding',
      'type': 'uint',
      'values': ['03', '06', '09', '12', '15', '18', '21'],
      'affects': ['dist2', 'dist3', 'dist4']
    },
    {
      'code': 'gtt',
      'path': 'sender_config.params.give_token_threshold',
      'type': 'float',
      'values': ['0.05', '0.15', '0.25', '0.35', '0.45', '0.55'],
      'affects': ['dist2', 'dist4']
    },
    {
      'code': 'grt',
      'path': 'sender_config.params.give_rate_threshold',
      'type': 'float',
      'values': ['0.75', '0.80', '0.85', '0.90', '0.95'],
      'affects': ['dist3', 'dist4']
    }
  ]


def generateSettings(alg):
  # get raw settings, only keep those that affect this algorithm
  raw = [setting for setting in rawSettings() if alg in setting['affects']]

  index = [0] * len(raw)
  sizes = [len(x['values']) for x in raw]

  settings = []
  while True:
    # create code and override for this setting
    code = ''
    override = ''
    for dim, rr in enumerate(raw):
      # add to code
      if dim > 0:
        code += '_'
      code += rr['code'] + rr['values'][index[dim]]

      # add to override
      if dim > 0:
        override += ' '
      override += rr['path'] + '=' + rr['type'] + '=' + rr['values'][index[dim]]

    # add setting to list
    settings.append((code, override))

    # advance index
    done = True
    for dim in range(len(raw)):
      if index[dim] + 1 < sizes[dim]:
        index[dim] += 1
        done = False
        break
      else:
        index[dim] = 0
    if done:
      break

  return settings


def remove(force, filepath):
  if os.path.isfile(filepath):
    if force:
      os.remove(filepath)
    else:
      while True:
        ans = input('\'{0}\' already exists, remove it? (N/y) '
                    .format(filepath)).lower()
        if ans == '' or ans == 'no' or ans == 'n':
          print('then we can\'t proceed :(')
          sys.exit(-1)
        elif ans == 'yes' or ans == 'y':
          os.remove(filepath)
          break


def main(args):
  assert args.cores <= totalCores(), 'not enough cores'
  assert args.memory <= availableMemory(), 'not enough memory available'

  # create the task manager
  rm = taskrun.ResourceManager(
    taskrun.CounterResource('core', 9999999, args.cores),
    taskrun.MemoryResource('mem', 9999999, args.memory))
  tm = taskrun.TaskManager(
    resource_manager=rm,
    observer=taskrun.VerboseObserver(show_description=args.verbose),
    failure_mode=taskrun.FailureMode.ACTIVE_CONTINUE)

  # make batch directory
  if not os.path.isdir(args.batch):
    os.mkdir(args.batch)

  # find json files matching the regex
  for ff in os.listdir('json'):
    ff = os.path.join('json', ff)
    alg = getName(ff)
    if (os.path.isfile(ff) and
        ff.endswith('.json') and
        re.search(args.regex, alg)):
      # generate all settings combinations for this algorithm
      settings = generateSettings(alg)

      # apply all settings combinations to each json file
      for code, override in settings:
        # get all names and files
        io = getFiles(ff, args.batch, alg, code)
        if args.verbose:
          print(io['id'])

        # determine what needs to run
        run_sim = not os.path.isfile(io['log'])
        run_plot = (run_sim or
                    (args.plot and not os.path.isfile(io['plot'])) or
                    (args.data and not os.path.isfile(io['data'])))

        # create sim task
        sim = None
        if run_sim:
          sim_cmd = 'bin/ratesim {0} log_file=string={1} {2}'.format(
            io['json'], io['log'], override)
          sim = taskrun.ProcessTask(
            tm, 'sim_{0}'.format(io['id']), sim_cmd)
          sim.resources = {'core': 1, 'mem': 1}

        # create plot task
        if run_plot:
          plot_cmd = 'parser/run.py {0}'.format(io['log'])
          if args.plot:
            plot_cmd += ' -s {0} -p {1}'.format(args.smoothing, io['plot'])
          if args.data:
            plot_cmd += ' -d {0}'.format(io['data'])
          plot = taskrun.ProcessTask(tm, 'plot_{0}'.format(io['id']), plot_cmd)
          plot.resources = {'core': 1, 'mem': 2}
          if sim:
            plot.add_dependency(sim)

  # run all tasks
  tm.run_tasks()


if __name__ == '__main__':
  ap = argparse.ArgumentParser()
  ap.add_argument('batch',
                  help='name of the batch (directory)')
  ap.add_argument('regex',
                  help='a regex to match config file basenames')
  ap.add_argument('-s', '--smoothing', type=int, default=50,
                  help='plot line smoothing factor')
  ap.add_argument('-c', '--cores', type=int,
                  default=totalCores(),
                  help='number of cores to use during run')
  ap.add_argument('-m', '--memory', type=float,
                  default=availableMemory(0.95),
                  help='amount of memory (in GiB) to use during run')
  ap.add_argument('-p', '--plot', action='store_true',
                  help='generate plot files')
  ap.add_argument('-d', '--data', action='store_true',
                  help='generate data files')
  ap.add_argument('-v', '--verbose', action='store_true',
                  help='enable verbose output')
  args = ap.parse_args()
  sys.exit(main(args))
