import numpy as np
import matplotlib.pyplot as plt

data = []

with open('output_withvalue_30k.txt', 'r') as f:
    lines = f.read().split('\n')

    for line in lines:
        data.append(list(map(float, line.split())))



data = np.array(data[:-1])
print(data[:, 2:].sum())
print(data[:, 1].max())

plt.scatter(data[:, 1], data[:, 3])
plt.show()

