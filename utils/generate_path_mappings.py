from yaml import dump

yaml_array = []
for i in range(1, 257):
    yaml_data = {
        "name": f"{i + 256}",
        "paths": [
            f"/home/admin/eclipse-workspace/ArgoUMLSPLBenchmark/scenarios/ScenarioAllVariants/variants/00{i:03d}.config/src/"
        ],
    }
    yaml_array.append(yaml_data)
print(dump(yaml_array, None, sort_keys=False))
