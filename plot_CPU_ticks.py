import seaborn as sns
import matplotlib.pyplot as plt

file1 = open('log.txt', 'r') 
Lines = file1.readlines() 

arr_cid = []
arr_ints = []
arr_ticks = []

for line in Lines:
    if "YEEETCPU:" not in line:
        continue
    # print(line)
    temp = line.split(" ")
    x, y, z = int(temp[1]), int(temp[2]), int(temp[3])
    # if( x < 7 and int(z)%50 < 2):
    arr_cid.append(x)
    arr_ticks.append(y)
    arr_ints.append(z)
    

temp_dict = {
    "CPU ID": arr_cid,
    "CPU interrupts": arr_ints,
    "Ticks": arr_ticks,
}

sns_plot = sns.lineplot(data=temp_dict, x="Ticks", y="CPU interrupts", hue="CPU ID",  palette="deep")
# plt.legend(bbox_to_anchor=(1.05, 1), loc=2, borderaxespad=0.)
sns_plot = sns_plot.get_figure()
sns_plot.savefig("output_cpu_ints.png")

plt.clf()

# sns_plot = sns.lineplot(data=temp_dict, x="time", y="Current queue", hue="pid", style="pid",
#     markers=True, dashes=False, palette="deep")
# # plt.legend(bbox_to_anchor=(1.05, 1), loc=2, borderaxespad=0.)
# sns_plot = sns_plot.get_figure()
# sns_plot.savefig("output.png")

# plt.clf()

# sns_plot = sns.scatterplot(data=temp_dict, x="time", y="Current queue", hue="pid", palette="deep")
# sns_plot = sns_plot.get_figure()
# sns_plot.savefig("output_scatter.png")