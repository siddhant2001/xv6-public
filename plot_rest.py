import seaborn as sns
import matplotlib.pyplot as plt

file1 = open('log.txt', 'r') 
Lines = file1.readlines() 

arr_pid = []
arr_status = []
arr_tick = []

for line in Lines:
    if "YEEET:" not in line:
        continue
    # print(line)
    try:
        temp = line.split(" ")
        x, y, z = int(temp[1]), float(temp[2]), temp[3]
        arr_pid.append(x)
        arr_tick.append(y)
        arr_status.append(z)
    except:
        pass

temp_dict = {
    "pid": arr_pid,
    "state": arr_status,
    "time": arr_tick,
}

sns_plot = sns.lineplot(data=temp_dict, x="time", y="pid", hue="state")
# plt.legend(bbox_to_anchor=(1.05, 1), loc=2, borderaxespad=0.)
sns_plot = sns_plot.get_figure()
sns_plot.savefig("graphs/output_lineplot_for_all.png")

# plt.clf()

# sns_plot = sns.lineplot(data=temp_dict, x="time", y="pid")
# # plt.legend(bbox_to_anchor=(1.05, 1), loc=2, borderaxespad=0.)
# sns_plot = sns_plot.get_figure()
# sns_plot.savefig("output.png")

plt.clf()

sns_plot = sns.scatterplot(data=temp_dict, x="time", y="pid", hue="state")
sns_plot = sns_plot.get_figure()
sns_plot.savefig("graphs/output_scatter_for_all.png")