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

  # create a quad-plot
  fig = plt.figure(figsize=(16,10))
  ax1 = fig.add_subplot(2, 2, 1)
  ax2 = fig.add_subplot(2, 2, 2)
  ax3 = fig.add_subplot(2, 2, 3)
  ax4 = fig.add_subplot(2, 2, 4)

  # get the xlim to be used by all time-based plots
  xlim = getXlim(raw)

  # plot data
  bulkAggregates(ax1, raw, xlim, smoothness=args.smoothness)
  estimatedBisectionBandwidth(ax2, raw, xlim, smoothness=args.smoothness)
  latencyScatter(ax3, raw, xlim)

  #
  fig.tight_layout()
  plt.show()

  png = 'quadplot.png'
  print('saving {0}'.format(png))
  fig.savefig(png)


def getXlim(raw):
  # extract latency data
  times, _ = raw.extractLatencies()
  return max(times) * 1.02


"""
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
"""

def bulkAggregates(ax, raw, xlim, smoothness):
  # extract the data
  ss = raw.extractRate(['Sender_*'], True)
  sr = raw.extractRate(['Sender_*'], False)
  rr = raw.extractRate(['Receiver_*'], False)

  # apply smoothing
  ssb = smooth(ss, smoothness)
  srb = smooth(sr, smoothness)
  rrb = smooth(rr, smoothness)

  # plot the data
  time = numpy.arange(raw.stats['Total simulation ticks'] + 1)
  ssl = ax.plot(time, ssb, 'r', label='Senders send')
  srl = ax.plot(time, srb, 'g', label='Senders recv')
  rrl = ax.plot(time, rrb, 'b', label='Receivers recv')

  # format axis
  ax.set_xlabel('Time (cycles)')
  ax.set_ylabel('Rate (flits/cycle)')
  ax.set_xlim(0, xlim)
  ax.set_ylim([0, max(rrb) * 1.50])
  ax.legend()
  ax.set_title('Bulk Rates')


def estimatedBisectionBandwidth(ax, raw, xlim, smoothness):
  # extract the data
  rate = raw.extractRate(['.*'], False)

  # estimate the amount of traffic that goes over the bisection
  bisection_fraction = 0.5
  for idx in range(len(rate)):
    rate[idx] /= bisection_fraction

  # apply smoothing
  rate = smooth(rate, smoothness)

  # plot the data
  time = numpy.arange(raw.stats['Total simulation ticks'] + 1)
  line = ax.plot(time, rate, 'r', label='Bisection Bandwidth')

  # format axis
  ax.set_xlabel('Time (cycles)')
  ax.set_ylabel('Bisection Bandwidth (flits/cycle)')
  ax.set_title('Bisection Bandwidth')
  ax.set_xlim(0, xlim)


def latencyScatter(ax, raw, xlim):
  # extract latency data
  times, latencies = raw.extractLatencies()

  # scatter plot the data
  sc = ax.scatter(times, latencies, color='b', s=2)

  # format axis
  ax.set_xlabel('Time (cycles)')
  ax.set_ylabel('Latency (cycles)')
  ax.set_title('Transaction Latencies')
  ax.set_xlim(0, xlim)


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
  ap.add_argument('-s', '--smoothness', type=int, default=0,
                  help='smoothing factor applied to all data')
  main(ap.parse_args())
