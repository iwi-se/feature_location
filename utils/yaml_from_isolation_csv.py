import re
import sys
import yaml

class FlowList(list):
    pass

def represent_flow_list(dumper, data):
    return dumper.represent_sequence('tag:yaml.org,2002:seq', data, flow_style=True)

yaml.add_representer(FlowList, represent_flow_list)

def parse_difference_expression(expression: str):
    left_part, right_part = expression.split(' \\ ')
    left_numbers = re.findall(r'\d+', left_part)
    right_numbers = re.findall(r'\d+', right_part)
    left_numbers = list(map(int, left_numbers))
    right_numbers = list(map(int, right_numbers))
    return left_numbers, right_numbers

def generate_yaml_from_isolation_result(file, output_file):
    data = []
    with open(file, "r") as f:
        is_after_isolation_headline = False
        for line in f:
            if is_after_isolation_headline and not "FEATURE ID" in line:
                print(line)
                cols = line.split("\t")
                data.append({
                    "feature_id": cols[0],
                    "feature_name": cols[1],
                    "isolation_result": cols[2],
                    "number_min_differences": cols[3],
                    "min_differences": cols[4:]
                })
            if "=== ISOLATION RESULTS ===" in line:
                is_after_isolation_headline = True

    yaml_data = []
    for feature in data:
        for i, min_difference in enumerate(feature["min_differences"]):
            if min_difference.strip() == "":
                continue
            print(min_difference)
            left_numbers, right_numbers = parse_difference_expression(min_difference)
            yaml_data.append({
                "left-side": FlowList(left_numbers),
                "right-side": FlowList(right_numbers),
                "labels": [feature["feature_id"], feature["feature_name"], feature["feature_name"] + "_" + str(i)]
            })

    with open(output_file, "w") as f:
        yaml.dump({"expressions": yaml_data}, f, sort_keys=False)

if __name__ == "__main__":
    output_file = sys.argv[2] if len(sys.argv) > 2 else "from_isolation_csv.yaml"
    generate_yaml_from_isolation_result(sys.argv[1], output_file)