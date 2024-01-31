import numpy as np

count = 0

with open("EJK_G12_S20_LR.dat","r") as file:
	for line in file.readlines():
		if line.startswith("#"): continue
		line_s = line.split(" ")
		l = float(line_s[0])
		b = float(line_s[1])
		if l>-3.1 and l<3.1 and (b>-1.8 and b<1.8):
			count+=1

print(f"There are {count} grid points within the JASMINE extended footprint")
