import cv2
import numpy as np

cap = cv2.VideoCapture("input2.mp4")

while True:
    ret, frame = cap.read()
    if not ret:
        break

    frame = cv2.resize(frame, (640, 360))
    h, w = frame.shape[:2]

    # -------------------------------
    # Triangle (same as before)
    # -------------------------------
    triangle = np.array([
        [0, h],
        [w, h],
        [w//2, int(h*0.5)]
    ])

    overlay = frame.copy()
    cv2.fillPoly(overlay, [triangle], (0, 255, 0))
    output = cv2.addWeighted(overlay, 0.3, frame, 0.7, 0)

    # -------------------------------
    # Center Line
    # -------------------------------
    center_x = w // 2
    cv2.line(output, (center_x, 0), (center_x, h), (255, 0, 255), 2)

    # -------------------------------
    # Split Regions (geometry same)
    # -------------------------------
    left_screen = np.array([
        [0, h],
        [center_x, h],
        [center_x, int(h*0.5)]
    ])

    right_screen = np.array([
        [center_x, h],
        [w, h],
        [center_x, int(h*0.5)]
    ])

    overlay_lr = output.copy()

    # IMPORTANT: Colors now represent REAL CAR SIDES
    # Screen LEFT → Car RIGHT (Blue)
    cv2.fillPoly(overlay_lr, [left_screen], (255, 0, 0))

    # Screen RIGHT → Car LEFT (Red)
    cv2.fillPoly(overlay_lr, [right_screen], (0, 0, 255))

    output = cv2.addWeighted(overlay_lr, 0.2, output, 0.8, 0)

    # -------------------------------
    # Correct Labels (REAL WORLD)
    # -------------------------------
    cv2.putText(output, "RIGHT",
                (40, h - 20),
                cv2.FONT_HERSHEY_SIMPLEX,
                0.7, (255, 0, 0), 2)

    cv2.putText(output, "LEFT",
                (w - 150, h - 20),
                cv2.FONT_HERSHEY_SIMPLEX,
                0.7, (0, 0, 255), 2)

    cv2.putText(output, "CENTER (REAR AXIS)",
                (center_x - 110, 30),
                cv2.FONT_HERSHEY_SIMPLEX,
                0.5, (255, 0, 255), 1)

    cv2.imshow("Rear View Corrected Mapping", output)

    if cv2.waitKey(1) & 0xFF == 27:
        break

cap.release()
cv2.destroyAllWindows()