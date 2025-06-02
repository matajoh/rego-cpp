from argparse import ArgumentParser
import json
import os
from typing import Mapping


Null = tuple(["rego", "null"])


class UndefinedError(Exception):
    pass


class BreakError(Exception):
    def __init__(self, levels: int):
        self.levels = levels


class VM:
    def __init__(self, policy_path: str, data_path: str, input_path: str = None, verbose: bool = False):
        with open(policy_path, "r") as f:
            policy = json.load(f)

        self.verbose = verbose
        self.indent = ""
        self.stack = []
        self.result_set = []
        self.func_return = None
        self.strings = [
            s["value"] for s in policy["static"]["strings"]
        ]
        self.plans = {
            p["name"]: p
            for p in policy["plans"]["plans"]
        }
        self.functions = {
            f["name"]: f
            for f in policy["funcs"]["funcs"]
        }

        if input_path is not None:
            with open(input_path, "r") as f:
                self.input_data = json.load(f)
        else:
            self.input_data = {}

        if data_path is not None:
            with open(data_path, "r") as f:
                self.data = json.load(f)
        else:
            self.data = {}

    def read_local(self, key):
        if not self.stack:
            raise ValueError("Stack is empty")
        if key not in self.stack[-1]:
            return None
        return self.stack[-1][key]

    def write_local(self, key, value, once=False):
        if not self.stack:
            raise ValueError("Stack is empty")
        if once and key in self.stack[-1]:
            raise ValueError(f"Local variable '{key}' already set")
        self.stack[-1][key] = value

    def is_defined(self, key):
        if not self.stack:
            raise ValueError("Stack is empty")
        return key in self.stack[-1]

    def reset_local(self, key):
        if not self.stack:
            raise ValueError("Stack is empty")
        if key in self.stack[-1]:
            del self.stack[-1][key]

    def unpack_operand(self, operand):
        match operand["type"]:
            case "local":
                return self.read_local(operand["value"])
            case "bool":
                return operand["value"] == "true"
            case "string_index":
                index = int(operand["value"])
                return self.strings[index]

    def run(self, plan_name: str):
        self.result_set.clear()
        self.stack.clear()
        self.stack.append({0: self.input_data, 1: self.data})
        plan = self.plans[plan_name]
        for block in plan["blocks"]:
            self.run_block(block)

        return self.result_set

    def run_block(self, block: Mapping):
        for stmt in block["stmts"]:
            try:
                self.run_stmt(stmt)
            except UndefinedError:
                return

    def run_stmt(self, stmt: Mapping):
        if self.verbose:
            print(f"{self.indent}Running statement: {stmt['type']}")

        match stmt["type"]:
            case "MakeObjectStmt":
                self.write_local(stmt["stmt"]["target"], {})
            case "MakeArrayStmt":
                self.write_local(stmt["stmt"]["target"], [])
            case "MakeNullStmt":
                self.write_local(stmt["stmt"]["target"], Null)
            case "MakeNumberRefStmt":
                value = self.strings[int(stmt["stmt"]["Index"])]
                try:
                    value = int(value)
                except ValueError:
                    value = float(value)
                self.write_local(stmt["stmt"]["target"], value)
            case "BlockStmt":
                for block in stmt["stmt"]["blocks"]:
                    self.run_block(block)
            case "CallStmt":
                func_name = stmt["stmt"]["func"]
                args = stmt["stmt"]["args"]
                value = self.call_function(func_name, args)
                if value is not None:
                    self.write_local(stmt["stmt"]["result"], value)
            case "ResetLocalStmt":
                self.reset_local(stmt["stmt"]["target"])
            case "AssignVarOnceStmt":
                source = self.unpack_operand(stmt["stmt"]["source"])
                self.write_local(stmt["stmt"]["target"], source, True)
            case "IsDefinedStmt":
                if not self.is_defined(stmt["stmt"]["source"]):
                    raise UndefinedError
            case "ReturnLocalStmt":
                self.func_return = self.read_local(stmt["stmt"]["source"])
            case "ObjectInsertStmt":
                key = self.unpack_operand(stmt["stmt"]["key"])
                value = self.unpack_operand(stmt["stmt"]["value"])
                obj = self.read_local(stmt["stmt"]["object"])
                if obj is not None:
                    obj[key] = value
            case "ArrayAppendStmt":
                value = self.unpack_operand(stmt["stmt"]["value"])
                array = self.read_local(stmt["stmt"]["array"])
                if array is not None:
                    array.append(value)
            case "DotStmt":
                obj = self.unpack_operand(stmt["stmt"]["source"])
                key = self.unpack_operand(stmt["stmt"]["key"])
                if isinstance(obj, dict) and key not in obj:
                    raise UndefinedError

                value = obj[key]
                self.write_local(stmt["stmt"]["target"], value)
            case "AssignVarStmt":
                value = self.unpack_operand(stmt["stmt"]["source"])
                self.write_local(stmt["stmt"]["target"], value)
            case "ResultSetAddStmt":
                value = self.read_local(stmt["stmt"]["value"])
                if value is not None:
                    self.result_set.append(value)
            case _:
                print(f"Unsupported type: {stmt['type']}")

    def call_function(self, func_name: str, args: list):
        if self.verbose:
            print(f"{self.indent}Calling function: {func_name}")

        self.indent += "  "
        if func_name not in self.functions:
            raise ValueError(f"Function {func_name} not found")

        frame = {}
        func = self.functions[func_name]

        for i, arg in zip(func["params"], args):
            frame[i] = self.unpack_operand(arg)

        self.stack.append(frame)
        for block in func["blocks"]:
            self.run_block(block)

        value = self.read_local(func["return"])
        self.stack.pop()
        self.indent = self.indent[:-2]
        if self.verbose:
            print(f"{self.indent}Function {func_name} returned: {value}")

        return value


if __name__ == "__main__":
    parser = ArgumentParser(description="Run IRVM with a given plan and data.")
    parser.add_argument("bundle", help="Path to the bundle directory.")
    parser.add_argument("plan", help="Name of the plan to run.")
    parser.add_argument("--input", "-i", help="Path to a input JSON file.")
    parser.add_argument("--verbose", "-v", action="store_true", help="Enable verbose output.")
    args = parser.parse_args()

    policy_path = os.path.join(args.bundle, "plan.json")
    data_path = os.path.join(args.bundle, "data.json")
    vm = VM(policy_path, data_path, args.input, args.verbose)
    print(vm.run(args.plan))
