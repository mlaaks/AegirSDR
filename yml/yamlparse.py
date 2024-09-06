#!/usr/bin/python3
import yaml
class parseyaml:
	def __init__(self):
		self.yamldata=[]

	def loadconfig(self):
		with open('config.yml', 'r') as file:
			my_dict = yaml.safe_load(file)
		self.yamldata=my_dict
		#return my_dict.get('parameters')
	def printit(self):
		print(self.yamldata)
		return self.yamldata


if __name__ == '__main__':
	obj = parseyaml()
	obj.loadconfig()
	obj.printit()
	