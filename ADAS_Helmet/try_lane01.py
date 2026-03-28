import cv2
import numpy as np
from ultralytics import YOLO

model = YOLO("yolov8n.pt")

cap = cv2.VideoCapture("input2.mp4")

K = 800  

distance_history = {}
alerted_ids = set()   # NEW

while True:
    ret, frame = cap.read()
    if not ret:
        break

    frame = cv2.resize(frame, (640, 360))
    h, w = frame.shape[:2]

    # TRIANGLE (UNCHANGED)
    triangle = np.array([
        [0, h],
        [w, h],
        [w//2, int(h*0.5)]
    ])

    overlay = frame.copy()
    cv2.fillPoly(overlay, [triangle], (0, 255, 0))
    output = cv2.addWeighted(overlay, 0.3, frame, 0.7, 0)

    # CENTER LINE
    center_x = w // 2
    cv2.line(output, (center_x, 0), (center_x, h), (255, 0, 255), 2)

    # YOLO TRACK
    results = model.track(frame, persist=True, imgsz=320, verbose=False)

    for r in results:
        if r.boxes is None:
            continue

        for box in r.boxes:

            cls = int(box.cls[0])
            conf = float(box.conf[0])
            label = model.names[cls]

            if label not in ["car", "truck", "bus", "motorbike"]:
                continue

            if conf < 0.4:
                continue

            if box.id is None:
                continue

            obj_id = int(box.id[0])

            x1, y1, x2, y2 = map(int, box.xyxy[0])

            # DISTANCE
            bbox_height = y2 - y1
            if bbox_height <= 0:
                continue

            distance = K / bbox_height

            # STORE HISTORY
            if obj_id not in distance_history:
                distance_history[obj_id] = []

            distance_history[obj_id].append(distance)

            if len(distance_history[obj_id]) > 5:
                distance_history[obj_id].pop(0)

            # -------------------------------
            # SINGLE ALERT LOGIC (FIXED)
            # -------------------------------
            if len(distance_history[obj_id]) >= 3:

                d1, d2, d3 = distance_history[obj_id][-3:]

                approaching = (d3 < d2 < d1)

                if approaching and obj_id not in alerted_ids:
                    print(f"[ALERT] Vehicle ID {obj_id} approaching | Distance: {d3:.2f} m")
                    alerted_ids.add(obj_id)

            # SIDE
            cx = (x1 + x2) // 2

            if cx < center_x:
                side = "CAR RIGHT"
                color = (255, 0, 0)
            else:
                side = "CAR LEFT"
                color = (0, 0, 255)

            # PROXIMITY
            if distance < 5:
                proximity = "VERY CLOSE"
            elif distance < 10:
                proximity = "CLOSE"
            else:
                proximity = "FAR"

            # DRAW
            cv2.rectangle(output, (x1, y1), (x2, y2), color, 2)

            cv2.putText(output,
                        f"{side} | {distance:.1f}m | {proximity}",
                        (x1, y1 - 5),
                        cv2.FONT_HERSHEY_SIMPLEX,
                        0.5, color, 2)

    # LABELS
    cv2.putText(output, "CAR RIGHT",
                (40, h - 20),
                cv2.FONT_HERSHEY_SIMPLEX,
                0.7, (255, 0, 0), 2)

    cv2.putText(output, "CAR LEFT",
                (w - 150, h - 20),
                cv2.FONT_HERSHEY_SIMPLEX,
                0.7, (0, 0, 255), 2)

    cv2.putText(output, "CENTER (REAR AXIS)",
                (center_x - 110, 30),
                cv2.FONT_HERSHEY_SIMPLEX,
                0.5, (255, 0, 255), 1)

    cv2.imshow("Rear Detection + Distance", output)

    if cv2.waitKey(1) & 0xFF == 27:
        break

cap.release()
cv2.destroyAllWindows()