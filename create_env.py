import os
Import("env")

if os.path.exists(".env"):
    with open(".env") as f:
        for line in f:
            if "=" in line and not line.startswith("#"):
                key, val = line.strip().split("=", 1)
                # This injects -D KEY="VALUE" into the C compiler
                env.Append(CPPDEFINES=[(key, env.StringifyMacro(val.strip('"')))])
