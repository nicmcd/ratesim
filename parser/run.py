#!/usr/bin/env python3

import argparse
import json
import rateparse
import matplotlib.pyplot as plt
import numpy

def main(args):
  # get the RawData object
  raw = rateparse.RawData(args.input)

  # if requested, save the raw data to a file
  if args.output:
    raw.write(args.output)

  # plot something
  bulkAggregate(raw)


def receiverAggregate(raw):
  rate = raw.extractRate(['Receiver_*'], False)

  fig = plt.figure(figsize=(16,10))
  ax1 = fig.add_subplot(1, 1, 1)

  line = raw.plotRate(ax1, rate)

  #ax1.legend()

  plt.show()


def senderAggregate(raw):
  rate = raw.extractRate(['Sender_*'], True)

  fig = plt.figure(figsize=(16,10))
  ax1 = fig.add_subplot(1, 1, 1)

  line = raw.plotRate(ax1, rate)

  #ax1.legend()

  plt.show()


def bulkAggregate(raw):
  ss = raw.extractRate(['Sender_*'], True)
  sr = raw.extractRate(['Sender_*'], False)
  rr = raw.extractRate(['Receiver_*'], False)

  for smoothness in range(0, 100+1, 10):
    ssb = smooth(ss, smoothness)
    srb = smooth(sr, smoothness)
    rrb = smooth(rr, smoothness)

    fig = plt.figure(figsize=(16,10))
    ax = fig.add_subplot(1, 1, 1)

    time = numpy.arange(raw.stats['Total simulation ticks'] + 1)

    ssl = ax.plot(time, ssb, 'r', label='Senders send')
    srl = ax.plot(time, srb, 'g', label='Senders recv')
    rrl = ax.plot(time, rrb, 'b', label='Receivers recv')

    ax.set_xlabel('Time (cycles)')
    ax.set_ylabel('Rate (flits/cycle)')
    ax.set_ylim([0, 40])

    ax.legend()
    ax.set_title('Rates (Smooth={0:03d})'.format(smoothness))

    if smoothness == 0:
      plt.show()

    png = 'rates_{0:03d}.png'.format(smoothness)
    print('saving {0}'.format(png))
    fig.savefig(png)


def smooth(data, radius):
  ndata = data.copy()
  size = len(ndata)

  # quick bail out when no work needed
  if radius < 1:
    return ndata

  # process each data point
  for idx in range(0, size):
    bot = max(0, idx - radius)
    top = min(size - 1, idx + radius)
    cnt = top + 1 - bot
    sum = 0
    #print('{0} {1} {2} {3}'.format(idx, bot, top, cnt))
    for idx2 in range(bot, top+1):
      sum += data[idx2]
    ndata[idx] = sum / cnt

  return ndata

if __name__ == '__main__':
  """
  data = [1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0,
          1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0,
          1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0,
          1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0]
  ndata = smooth(data, 4)
  print('{0}\n{1}'.format(data, ndata))
  import sys
  sys.exit(0)
  """

  ap = argparse.ArgumentParser()
  ap.add_argument('input',
                  help='input data or log file')
  ap.add_argument('-o', '--output',
                  help='save raw data to file')
  main(ap.parse_args())
