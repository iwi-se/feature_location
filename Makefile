requirements:
	pip install -r requirements.txt

test:
	python3 feature_location/feature_location_cli.py intersection example/with_not_features/s5_i.hpp example/with_not_features/s8_icl.hpp

show_ast:
	python3 feature_location/feature_location_cli.py show_ast example/with_not_features/s8_icl.hpp

example_difference_logging:
	python3 feature_location/feature_location_cli.py difference example/with_not_features/s2_rl.hpp example/with_not_features/s8_icl.hpp -- example/with_not_features/s1_r.hpp example/with_not_features/s7_ic.hpp

example_difference_checked:
	python3 feature_location/feature_location_cli.py difference example/with_not_features/s3_rc.hpp example/with_not_features/s8_icl.hpp -- example/with_not_features/s1_r.hpp example/with_not_features/s6_il.hpp

example_difference_iterative:
	python3 feature_location/feature_location_cli.py difference example/with_not_features/s5_i.hpp example/with_not_features/s8_icl.hpp -- example/with_not_features/s1_r.hpp example/with_not_features/s4_rcl.hpp

example_difference_logging_or_checked:
	python3 feature_location/feature_location_cli.py difference example/with_not_features/s2_rl.hpp example/with_not_features/s3_rc.hpp example/with_not_features/s4_rcl.hpp example/with_not_features/s7_ic.hpp example/with_not_features/s8_icl.hpp example/with_not_features/s6_il.hpp -- example/with_not_features/s1_r.hpp example/with_not_features/s5_i.hpp

example_file:
	python3 feature_location/feature_location_cli.py file example-config.yaml

.PHONY: requirements test example_difference_logging example_difference_checked example_difference_iterative example_difference_logging_or_checked example_file