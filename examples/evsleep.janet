(defn worker
  "Run for a number of iterations."
  [name iterations]
  (for i 0 iterations
    (ev/sleep 1)
    (print "worker " name " iteration " i)))

(ev/call worker :a 10)
(ev/sleep 0.2)
(ev/call worker :b 5)
(ev/sleep 0.3)
(ev/call worker :c 12)
