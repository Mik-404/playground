import sys
import yaml
import termcolor

def error(msg):
    print(termcolor.colored(msg, color="red", attrs=["bold"]), file=sys.stderr)
    exit(1)

ALLOWED_TYPES = ["string", "bool", "int"]

global_config = {}
required_keys = []

for fname in sys.argv[2:]:
    with open(fname) as f:
        conf = yaml.load(f, Loader=yaml.Loader)
        for k, v in conf.items():
            if k in global_config:
                error(f"duplicate key {k} in {fname}")
            if v["type"] not in ALLOWED_TYPES:
                error(f"invalid type '{v['type']}' for config key {k}")

            global_config[k] = v
            required = v.get("required")
            default = v.get("default")

            if required:
                required_keys.append(k)
            elif default is None:
                error(f"config key {k} is not required and default value is not set")

def emit_conf(k, v):
    assert isinstance(v, str)
    print(f"CONFIG_{k}={v}")
    print(f"CXXFLAGS += -DCONFIG_{k}={v}")
    print(f"CFLAGS += -DCONFIG_{k}={v}")
    print(f"ASFLAGS += -DCONFIG_{k}={v}")
    print()

with open(sys.argv[1]) as f:
    values = yaml.load(f, Loader=yaml.Loader)

for r in required_keys:
    if r not in values:
        error(f"required config {k} not set")

print("CXXFLAGS=")
print("CFLAGS=")
print("ASFLAGS=")

for k, v in values.items():
    if k not in global_config:
        error(f"unknown configuration key {k}")
    conf = global_config[k]
    if conf["type"] == "bool":
        if not isinstance(v, bool):
            error(f"expected boolean value, got {v.__class__.__name__}")
        if v:
            emit_conf(k, "1")
    elif conf["type"] == "int":
        if not isinstance(v, int):
            error(f"expected integer value, got {v.__class__.__name__}")
        emit_conf(k, str(v))
    elif conf["type"] == "string":
        enum = conf.get("enum")
        if enum is not None and v not in enum:
            error(f"value of {k} should be in {enum}, got '{v}'")
        emit_conf(k, v)
    else:
        assert False

for k, v in global_config.items():
    if k in values:
        continue
    if v["type"] == "bool":
        if v["default"]:
            emit_conf(k, "1")
    else:
        emit_conf(k, v["default"])
