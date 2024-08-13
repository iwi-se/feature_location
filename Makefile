requirements:
	pip install -r requirements.txt

test:
	python3 feature_location/feature_location.py intersection example/with_not_features/s2_rl.hpp example/with_not_features/s4_rcl.hpp

test_difference_logger:
	python3 feature_location/feature_location.py difference example/with_not_features/s2_rl.hpp example/with_not_features/s8_icl.hpp -- example/with_not_features/s1_r.hpp example/with_not_features/s7_ic.hpp

test_difference_check:
	python3 feature_location/feature_location.py difference example/with_not_features/s3_rc.hpp example/with_not_features/s8_icl.hpp -- example/with_not_features/s1_r.hpp example/with_not_features/s6_il.hpp

test_difference_iterative:
	python3 feature_location/feature_location.py difference example/with_not_features/s5_i.hpp example/with_not_features/s8_icl.hpp -- example/with_not_features/s1_r.hpp example/with_not_features/s4_rcl.hpp


.PHONY: requirements test test_differences test_difference_logger test_difference_check test_difference_iterative