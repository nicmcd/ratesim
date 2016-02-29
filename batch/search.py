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
from stats import *


def main(args):
  if args.verbose:
    print(args)

  # make batch directory
  assert os.path.isdir(args.batch)

  # keep track of the smallest
  bestValue = float('inf')
  bestFile = None

  # find data files matching the regex
  searchCount = 0
  for ff in os.listdir(args.batch):
    ff = os.path.join(args.batch, ff)
    if (os.path.isfile(ff) and
        ff.endswith('.txt') and
        re.search(args.regex, getName(ff))):
      searchCount += 1

      # print the matching file name
      if args.verbose:
        print('file \'{0}\' matched'.format(ff))

      # parse the file
      stats = parseStats(ff, args.verbose)

      # compare against current best
      if stats[args.sect][args.stat] < bestValue:
        if args.verbose:
          print('old={0} new={1} file={2}'.format(
            bestValue, stats[args.sect][args.stat], ff))
        bestValue = stats[args.sect][args.stat]
        bestFile = ff

  # print the best finding
  print('the best section {0} {1} value is: {2} ({3})'
        .format(args.sect, args.stat, bestFile, bestValue))
  if args.all:
    with open(bestFile, 'r') as fd:
      print(fd.read())
  print('{0} configurations searched'.format(searchCount))


if __name__ == '__main__':
  ap = argparse.ArgumentParser()
  ap.add_argument('batch',
                  help='name of the batch (directory)')
  ap.add_argument('regex',
                  help='a regex to match data file basenames')
  ap.add_argument('sect', type=int,
                  choices=[1, 2, 3],
                  help='the section to use for comparison')
  ap.add_argument('stat', type=str,
                  choices=['bw', '2-9s', '3-9s', '4-9s', '5-9s'],
                  help='the statistic to use for comparison')
  ap.add_argument('-v', '--verbose', action='store_true',
                  help='enable verbose output')
  ap.add_argument('-a', '--all', action='store_true',
                  help='show all stats of winner')
  args = ap.parse_args()
  sys.exit(main(args))
