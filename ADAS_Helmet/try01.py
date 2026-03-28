import cv2
import numpy as np

cap = cv2.VideoCapture("input2.mp4")

while True:
    ret, frame = cap.read()
    if not ret:
        break

    # -------------------------------
    # Resize (BIG SPEED BOOST)
    # -------------------------------
    frame = cv2.resize(frame, (640, 360))
    h, w = frame.shape[:2]

    # -------------------------------
    # ROI (only bottom half)
    # -------------------------------
    roi = frame[int(h*0.5):h, :]

    gray = cv2.cvtColor(roi, cv2.COLOR_BGR2GRAY)
    blur = cv2.GaussianBlur(gray, (5, 5), 0)
    edges = cv2.Canny(blur, 50, 120)

    # -------------------------------
    # Hough (light)
    # -------------------------------
    lines = cv2.HoughLinesP(
        edges,
        1,
        np.pi/180,
        threshold=30,
        minLineLength=40,
        maxLineGap=50
    )

    left_slopes = []
    left_intercepts = []

    right_slopes = []
    right_intercepts = []

    # -------------------------------
    # Collect slopes
    # -------------------------------
    if lines is not None:
        for l in lines:
            x1, y1, x2, y2 = l[0]

            if x2 == x1:
                continue

            slope = (y2 - y1) / (x2 - x1)

            if abs(slope) < 0.3:
                continue

            intercept = y1 - slope * x1

            if slope < 0:
                left_slopes.append(slope)
                left_intercepts.append(intercept)
            else:
                right_slopes.append(slope)
                right_intercepts.append(intercept)

    # -------------------------------
    # Average lines (KEY TRICK)
    # -------------------------------
    def make_line(slopes, intercepts):
        if len(slopes) == 0:
            return None
        m = np.mean(slopes)
        c = np.mean(intercepts)
        return m, c

    left_line = make_line(left_slopes, left_intercepts)
    right_line = make_line(right_slopes, right_intercepts)

    output = frame.copy()

    # -------------------------------
    # Draw Ego Lane Triangle
    # -------------------------------
    if left_line and right_line:

        m1, c1 = left_line
        m2, c2 = right_line

        # Bottom points
        y_bottom = h
        xl = int((y_bottom - c1) / m1)
        xr = int((y_bottom - c2) / m2)

        # Vanishing point (approx)
        if abs(m1 - m2) > 1e-3:
            xv = int((c2 - c1) / (m1 - m2))
            yv = int(m1 * xv + c1)

            # Adjust because ROI shifted
            yv += int(h*0.5)

            # Draw triangle
            pts = np.array([
                [xl, h],
                [xr, h],
                [xv, yv]
            ])

            cv2.fillPoly(output, [pts], (0, 255, 0))
            output = cv2.addWeighted(output, 0.3, frame, 0.7, 0)

            # Draw lines
            cv2.line(output, (xl, h), (xv, yv), (0,255,0), 2)
            cv2.line(output, (xr, h), (xv, yv), (0,255,0), 2)

            # Draw vanishing point
            cv2.circle(output, (xv, yv), 5, (0,0,255), -1)

    # Center line (debug)
    cv2.line(output, (w//2, 0), (w//2, h), (255,0,255), 1)

    cv2.imshow("Lightweight Lane Detection", output)

    if cv2.waitKey(1) & 0xFF == 27:
        break

cap.release()
cv2.destroyAllWindows()