import sys
import numpy as np
import matplotlib.pyplot as plt

with open(sys.argv[1]) as f:
	target_data = f.read()
target_data = target_data.rstrip("\n").split("\n")
tx = [row.split(' ')[0] for row in target_data]
ty = [row.split(' ')[1] for row in target_data]

with open(sys.argv[2]) as f:
	map2 = f.read()
map2 = map2.rstrip("\n").split("\n")
mx = [row.split(' ')[0] for row in map2]
my = [row.split(' ')[1] for row in map2]


fig = plt.figure()
plt.ylim([0, 150])
ax1 = fig.add_subplot(111)

ax1.set_xlabel("ADC")
ax1.set_ylabel("C")
ax1.set_autoscaley_on(False)
ax1.plot(tx, ty, c='g')

ax2 = ax1.twinx()
ax2.set_xlabel("ADC")
ax2.set_ylabel("C")
ax2.set_autoscaley_on(False)
plt.ylim([0, 150])
ax2.plot(mx, my, c='r')

plt.show()
