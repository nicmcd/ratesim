#!/usr/bin/env python3

import argparse
import math
import os
import psutil
import re
import shutil
import sys
import taskrun

from common import *
from penalty import *
from stats import *


def main(args):
  if args.verbose:
    print(args)

  # make batch directory
  assert os.path.isdir(args.batch)

  # keep track of the smallest
  minValue = float('inf')
  minFile = None

  # find data files matching the regex
  searchCount = 0
  for ff in os.listdir(args.batch):
    ff = os.path.join(args.batch, ff)
    if (os.path.isfile(ff) and
        ff.endswith('.txt') and
        re.search(args.regex, getName(ff))):
      # parse the file
      stats = parseStats(ff)
      if stats is None:
        continue

      searchCount += 1

      # extract the value
      if args.mode == 'single':
        value = stats[args.sect][args.stat]
      elif args.mode == 'penalty':
        value = computePenalty(stats)
      else:
        assert False, 'The programmer is an idiot!'

      # compare against current min
      if value < minValue:
        minValue = value
        minFile = ff

      # print the value
      if args.verbose:
        print('file \'{0}\' {1}: {2}'.format(
          ff, 'value' if args.mode == 'single' else 'penalty', value))

  # print the min finding
  print('{0} configurations searched'.format(searchCount))
  print('the file with the minimum value of {0} is:\n{1}'
        .format(minValue, minFile))
  if args.all:
    with open(minFile, 'r') as fd:
      print(fd.read())


if __name__ == '__main__':
  ap = argparse.ArgumentParser()
  ap.add_argument('batch',
                  help='name of the batch (directory)')
  ap.add_argument('regex',
                  help='a regex to match data file basenames')
  ap.add_argument('mode', choices=['single', 'penalty'],
                  help='optimize single metric or use penalty score')
  ap.add_argument('--sect', type=int, default=2,
                  choices=[1, 2, 3],
                  help='the section to use for comparison')
  ap.add_argument('--stat', type=str, default='4-9s',
                  choices=['bw', '2-9s', '3-9s', '4-9s', '5-9s'],
                  help='the statistic to use for comparison')
  ap.add_argument('-v', '--verbose', action='store_true',
                  help='enable verbose output')
  ap.add_argument('-a', '--all', action='store_true',
                  help='show all stats of winner')
  args = ap.parse_args()
  sys.exit(main(args))
