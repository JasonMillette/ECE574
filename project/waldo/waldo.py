#Program using openCV to filter red
#Jason Millette, Spencer Goulette
#4/17/19

#Uses template matching to find waldo

import cv2
import numpy as np

# Read images
original = cv2.imread("waldo2.jpeg", cv2.IMREAD_COLOR)
waldo = cv2.imread("posWaldo.jpeg", cv2.IMREAD_COLOR)
#w, h = waldo.shape[::-1]

#find matches
result = cv2.matchTemplate(original, waldo, cv2.TM_CCOEFF)
min_val, max_val, min_loc, mac_loc = cv2.minMaxLoc(result)

#box waldo
#topLeft = maxLoc
#botRight = (topLeft[0] + w, topLeft[1] + h)
#cv2.rectangle(original, top_left, bottom_right, 255, 2)

#write out image
cv2.imwrite("out.jpeg", result)
#display
#cv2.imshow("result", result)
#cv2.imshow("template", waldo)
#cv2.waitKey(0)
