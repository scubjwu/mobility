This is the program to replay real human movement trace. It works fine under large scale network. We try to record the mobility information as much as possible. For example, the visiting frequence of each position, neighbor distribution, pause time distribution and flight distribution of each node are collected.

Based on this program, any application or routing scheme could be easliy plugged in and to be evaluated.

My goal of this program is to look at how data flows based on real human mobility.

Currently the csv file format is like:
node-id,xpos,ypos,time,pos-id
(See the example csv file for more details)
