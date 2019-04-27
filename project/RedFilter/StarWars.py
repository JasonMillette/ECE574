#Program using openCV to filter red
#Jason Millette, Spencer Goulette
#4/17/19

import cv2
import numpy as np

# Read image
img = cv2.imread("StarWarsTheLastJedi.jpg", cv2.IMREAD_COLOR)

#Create numpy arrays for red
lower = np.array([0, 0, 150], dtype = "uint8")
upper = np.array([20, 20, 255], dtype = "uint8")

#mask, find the color red
mask = cv2.inRange(img, lower, upper)
output = cv2.bitwise_and(img, img, mask = mask)

#display
cv2.imshow("Original", img)
cv2.imshow("filtered", output)
cv2.waitKey(0)
