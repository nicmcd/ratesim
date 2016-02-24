#!/usr/bin/env python3

import argparse
import multiprocessing
import os
import re
import shutil
import sys
import taskrun

from common import *


def generateSetting(alg, **kwargs):
  # get all parameters
  params = parameters()

  # filter the parameters to only those that we care about
  params = [param for param in params if alg in param['affects']]

  # check that all kwargs exist in 'params'
  assert len(params) == len(kwargs), 'invalid number of kwargs for {0}'.format(alg)
  for kwarg in kwargs.keys():
    found = False
    for r in params:
      if kwarg == r['code']:
        found = True
        break
    if not found:
      print('{0} doesn\'t exist as a setting for {1}'.format(kwarg, alg))
      exit(-1)

  # apply all settings
  for r in params:
    r['value'] = kwargs[r['code']]

  # create code and override for this setting
  code = ''
  override = ''
  for dim, rr in enumerate(params):
    # add to code
    if dim > 0:
      code += '_'
    code += rr['code'] + rr['value']

    # add to override
    if dim > 0:
      override += ' '
    override += rr['path'] + '=' + rr['type'] + '=' + rr['value']

  # return the code and settings
  return (alg, code, override)


def generateSettingsForPaper():
  basic = generateSetting('basic')

  relay = generateSetting('relay', nr='500', mo='1000')

  dist1 = generateSetting('dist1', mt='1000')
  dist2 = generateSetting('dist2', mt='1000', st='0.50', tak='1.20', mro='18',
                          gtt='0.35')
  dist3 = generateSetting('dist3', mt='1000', st='0.50', rak='0.90', mro='21',
                          grt='0.75', grf='0.90')
  dist4 = generateSetting('dist4', mt='1000', st='0.50', tak='1.20', rak='0.90',
                          mro='21', gtt='0.15', grt='0.95', grf='0.90')

  return [basic, relay, dist1, dist2, dist3, dist4]


def main(args):
  # generate all settings for the paper
  settings = generateSettingsForPaper()

  # create the task manager
  rm = taskrun.ResourceManager(
    taskrun.CounterResource('core', 9999999, args.cores),
    taskrun.MemoryResource('mem', 9999999, args.memory))
  tm = taskrun.TaskManager(
    resource_manager=rm,
    observer=taskrun.VerboseObserver(),
    failure_mode=taskrun.FailureMode.ACTIVE_CONTINUE)

  # make batch directory
  if not os.path.isdir(args.batch):
    os.mkdir(args.batch)


  # create all tasks
  for alg, code, override in settings:
    # get all file names
    ff = os.path.join('json', '{0}.json'.format(alg))
    assert os.path.isfile(ff)
    io = getFiles(ff, args.batch, alg, code)

    if args.verbose:
      print(io['id'])

    # determine what needs to run
    run_sim = not os.path.isfile(io['log'])
    run_plot = (run_sim or
                (args.plot and not os.path.isfile(io['plot'])) or
                (args.data and not os.path.isfile(io['data'])))

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
  args = ap.parse_args()
  sys.exit(main(args))
