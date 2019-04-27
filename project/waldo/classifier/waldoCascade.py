#Program using openCV to filter red
#Jason Millette, Spencer Goulette
#4/27/19

#Uses s cascade classifier to detect waldo

import cv2
import numpy as np

# Read image
original = cv2.imread("../Images/waldo18.jpg", cv2.IMREAD_COLOR)

#Import classifier
waldo = cv2.CascadeClassifier("waldo.xml")

#Detects all instances of waldo
detected = waldo.detectMultiScale(original, scaleFactor=1.001, minNeighbors=120, minSize=(25,25), maxSize=(50,50))

#Draws a rectangle around the detected waldo instances
for (i, (x, y, w, h)) in enumerate(detected):
		cv2.rectangle(original, (x, y), (x+w, y+h), (255, 0, 0), 2)

#cv2.imshow("detected Waldos", original)

#writes waldo to file
cv2.imwrite("out.jpeg", original)
cv2.waitKey(0)
