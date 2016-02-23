#!/usr/bin/env python3

import os
if 'DISPLAY' not in os.environ or os.environ['DISPLAY'] == '':
  import matplotlib
  matplotlib.use('Agg')
import matplotlib.pyplot as plt
import numpy


res = 2000
cycles = numpy.arange(0, 130000, res)
group1 = numpy.zeros(len(cycles))
group2 = numpy.zeros(len(cycles))
limit = numpy.array([0.50] * len(cycles))

for index, cycle in enumerate(cycles):
  if cycle < 10000:
    group1[index] = 0.4
    group2[index] = 0.4
  elif cycle < 50000:
    group1[index] = 0.8
    group2[index] = 0.0
  elif cycle < 90000:
    group1[index] = 0.0
    group2[index] = 0.8
  else:
    group1[index] = 0.4
    group2[index] = 0.4

fig = plt.figure(figsize=(8,5))
ax = fig.add_subplot(1, 1, 1)

line1 = ax.plot(cycles, group1, 'b-+', label='Senders 1-500')
line2 = ax.plot(cycles, group2, 'r-x', label='Senders 501-1000')
line3 = ax.plot(cycles, limit, 'g--', label='Rate limit')

ax.set_xlabel('cycles')
ax.set_xlim([0, 130000])

ax.set_ylabel('individual injection rate')
ax.set_ylim([-0.05, 0.85])

ax.legend()

fig.tight_layout()

#plt.show()
fig.savefig('traffic.png')
