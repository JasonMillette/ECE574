#Program using openCV to filter red
#Jason Millette, Spencer Goulette
#4/17/19

#Uses template matching to find waldo

import cv2
import numpy as np

# Read images
original = cv2.imread("waldo2.jpeg", cv2.IMREAD_COLOR)
waldo = cv2.imread("templateWaldo.jpeg", cv2.IMREAD_COLOR)
w, h, channels = waldo.shape

#find matches
result = cv2.matchTemplate(original, waldo, cv2.TM_CCOEFF)
min_val, max_val, min_loc, max_loc = cv2.minMaxLoc(result)

#box waldo
top_left = max_loc
bot_right = (top_left[0] + w, top_left[1] + h)
cv2.rectangle(original, top_left, bot_right, 255, 2)

#write out image
cv2.imwrite("out.jpeg", original)
#display
#cv2.imshow("result", result)
#cv2.imshow("template", waldo)
#cv2.waitKey(0)
