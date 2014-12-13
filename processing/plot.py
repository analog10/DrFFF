import numpy as np
import matplotlib.pyplot as plt

# Code taken from http://stackoverflow.com/questions/21583965/matplotlib-cursor-value-with-two-axes
def make_format(current, other):
	# current and other are axes
	def format_coord(x, y):
		# x, y are data coordinates
		# convert to display coords
		display_coord = current.transData.transform((x,y))
		inv = other.transData.inverted()
		# convert back to data coords with respect to ax
		ax_coord = inv.transform(display_coord)
		coords = [ax_coord, (x, y)]
		return ('C: {:<40}    ADC: {:<}'.format(*['({:.3f}, {:.3f})'.format(x, y) for x,y in coords]))
	return format_coord

def on_pick(event):
	thisline = event.artist
	xdata, ydata = thisline.get_data()
	ind = event.ind
	print(ydata[ind[0]])
	#print('on pick line:', zip(xdata[ind], ydata[ind]))

with open("plot_target_bed.txt") as f:
	target_data = f.read()
target_data = target_data.rstrip("\n").split("\n")
tx = [row.split(' ')[0] for row in target_data]
ty = [row.split(' ')[1] for row in target_data]


with open("mm_bed_temp.txt") as f:
	mm_data = f.read()
mm_data = mm_data.rstrip("\n").split("\n")
mx = [row.split(' ')[0] for row in mm_data]
my = [row.split(' ')[1] for row in mm_data]

with open("plot_raw_adc.txt") as f:
	adc_data = f.read()
adc_data = adc_data.rstrip("\n").split("\n")
adc_x = [row.split(' ')[0] for row in adc_data]
adc_y = [row.split(' ')[1] for row in adc_data]

with open("plot_amb_temp.txt") as f:
	at_data = f.read()
at_data = at_data.rstrip("\n").split("\n")
at_x = [row.split(' ')[0] for row in at_data]
at_y = [row.split(' ')[1] for row in at_data]

with open("plot_amb_rh.txt") as f:
	rh_data = f.read()
rh_data = rh_data.rstrip("\n").split("\n")
rh_x = [row.split(' ')[0] for row in rh_data]
rh_y = [row.split(' ')[1] for row in rh_data]



fig = plt.figure()
ax1 = fig.add_subplot(111)

ax1.set_ylabel("RH %")
ax1.plot(rh_x,rh_y, c='c', label="Enclosure")
#leg3 = ax3.legend()


ax2 = ax1.twinx()
ax2.set_ylabel("C")
ax2.plot(tx,ty, 'bo', label="Target")
ax2.plot(at_x,at_y, c='y', label="Enclosure")

ax2.plot(mx,my, c='r', label="Bed Measured", picker=5)
leg2 = ax2.legend()

ax3 = ax2.twinx()
ax3.set_ylabel("ADC")
ax3.plot(adc_x,adc_y, c='g', label="Bed ADC", picker=5)
#leg2 = ax2.legend()

#ax2.set_zorder(0.1) 

fig.canvas.mpl_connect("pick_event", on_pick)

#ax2.format_coord = make_format(ax3, ax2)
plt.show()
