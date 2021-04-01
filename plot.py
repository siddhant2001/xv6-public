import seaborn as sns
import matplotlib.pyplot as plt

file1 = open('log.txt', 'r') 
Lines = file1.readlines() 

arr_pid = []
arr_curq = []
arr_tick = []

for line in Lines:
    if "YEEET:" not in line:
        continue
    # print(line)
    temp = line.split(" ")
    x, y, z = int(temp[1]), int(temp[2]), float(temp[3])
    # if( x < 7 and int(z)%50 < 2):
    arr_pid.append(x)
    arr_curq.append(y)
    arr_tick.append(z)
    

temp_dict = {
    "pid": arr_pid,
    "Current queue": arr_curq,
    "time": arr_tick,
}

sns_plot = sns.lineplot(data=temp_dict, x="time", y="Current queue", hue="pid",  palette="deep")
# plt.legend(bbox_to_anchor=(1.05, 1), loc=2, borderaxespad=0.)
sns_plot = sns_plot.get_figure()
sns_plot.savefig("output_temp.png")

plt.clf()

sns_plot = sns.lineplot(data=temp_dict, x="time", y="Current queue", hue="pid", style="pid",
    markers=True, dashes=False, palette="deep")
# plt.legend(bbox_to_anchor=(1.05, 1), loc=2, borderaxespad=0.)
sns_plot = sns_plot.get_figure()
sns_plot.savefig("output.png")

plt.clf()

sns_plot = sns.scatterplot(data=temp_dict, x="time", y="Current queue", hue="pid", palette="deep")
sns_plot = sns_plot.get_figure()
sns_plot.savefig("output_scatter.png")