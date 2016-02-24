#!/usr/bin/env python3

import argparse
import math
import os
import psutil
import shutil
import sys
import taskrun

from common import *


def generateSettings(alg):
  # get all parameters
  params = parameters()

  # set parameters value ranges
  for param in params:
    if param['code'] == 'nr':
      param['values'] = ['500', '750', '1000']
    elif param['code'] == 'mo':
      param['values'] = ['100', '150', '200', '1000', '10000', '100000']
    elif param['code'] == 'mt':
      param['values'] = ['1000', '2000']
    elif param['code'] == 'st':
      param['values'] = ['0.10', '0.30', '0.50', '0.70']
    elif param['code'] == 'tak':
      param['values'] = ['1.00', '1.20', '1.40', '1.60']
    elif param['code'] == 'rak':
      param['values'] = ['0.90', '1.00']
    elif param['code'] == 'mro':
      param['values'] = ['10', '20', '30', '40']
    elif param['code'] == 'gtt':
      param['values'] = ['0.05', '0.20', '0.35', '0.50']
    elif param['code'] == 'grt':
      param['values'] = ['0.90', '0.95']
    elif param['code'] == 'grf':
      param['values'] = ['0.90', '0.95']
    else:
      assert False, 'unknown param {0}'.format(param['code'])

  # filter the parameters to only those that we care about
  params = [param for param in params if alg in param['affects']]

  # use these variables for multi-dimensional array indexing
  index = [0] * len(params)
  sizes = [len(x['values']) for x in params]

  settings = []
  while True:
    # create code and override for this setting
    code = ''
    override = ''
    for dim, rr in enumerate(params):
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
    for dim in range(len(params)):
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
  # create the task manager
  rm = taskrun.ResourceManager(
    taskrun.CounterResource('core', 9999999, args.cores),
    taskrun.MemoryResource('mem', 9999999, args.memory))
  tm = taskrun.TaskManager(
    resource_manager=rm,
    observer=taskrun.VerboseObserver(description=args.verbose),
    failure_mode=taskrun.FailureMode.ACTIVE_CONTINUE)

  # make batch directory
  if not os.path.isdir(args.batch):
    os.mkdir(args.batch)

  # find json files matching the regex
  count = 0
  for ff in os.listdir('json'):
    ff = os.path.join('json', ff)
    alg = getName(ff)
    if (os.path.isfile(ff) and
        ff.endswith('.json') and
        alg in ['basic', 'relay', 'dist1', 'dist2', 'dist3', 'dist4']):
      # generate all settings combinations for this algorithm
      settings = generateSettings(alg)

      # apply all settings combinations to each json file
      for code, override in settings:
        count += 1

        # get all names and files
        io = getFiles(ff, args.batch, alg, code)
        if args.verbose:
          print(io['id'])

        # create sim task
        sim_cmd = 'bin/ratesim {0} log_file=string={1} {2}'.format(
          io['json'], io['log'], override)
        sim = taskrun.ProcessTask(
          tm, 'sim_{0}'.format(io['id']), sim_cmd)
        sim.resources = {'core': 1, 'mem': 2}
        sim.add_condition(taskrun.FileModificationCondition(
          [], [io['log']]))


        # create plot task
        plot_cmd = 'parser/parser.py {0} {1} {2} -s {3}'.format(
          io['log'], io['plot'], io['data'], args.smooth)
        plot = taskrun.ProcessTask(tm, 'plot_{0}'.format(io['id']), plot_cmd)
        plot.resources = {'core': 1, 'mem': 4}
        plot.add_dependency(sim)
        plot.add_condition(taskrun.FileModificationCondition(
          [io['log']], [io['plot'], io['data']]))


  # run all tasks
  print('{0} simulations'.format(count))
  if not args.debug:
    tm.run_tasks()


if __name__ == '__main__':
  cores = os.cpu_count()
  mem = taskrun.MemoryResource.current_available_memory_gib() * 0.95

  ap = argparse.ArgumentParser()
  ap.add_argument('batch',
                  help='name of the batch (directory)')
  ap.add_argument('-s', '--smooth', type=int, default=500,
                  help='plot line smoothing factor')
  ap.add_argument('-c', '--cores', type=int,
                  default=cores,
                  help='number of cores to use during run')
  ap.add_argument('-m', '--memory', type=float,
                  default=mem,
                  help='amount of memory (in GiB) to use during run')
  ap.add_argument('-v', '--verbose', action='store_true',
                  help='enable verbose output')
  ap.add_argument('-d', '--debug', action='store_true',
                  help='don\'t run simulations, just build')
  args = ap.parse_args()

  assert args.cores <= cores, 'not enough cores'
  assert args.memory <= mem, 'not enough memory available'

  sys.exit(main(args))
