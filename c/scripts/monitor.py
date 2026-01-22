from typing import Generator, Optional

import typer

import subprocess
import json
import select
import atexit

from collections import deque
import numpy as np


READ_COMMAND = [
    "thermo-cli",
    "fuse",
    "-C",
    "thermo_config.yaml",
    "--",
    "--power",
    "-s",
    "1",
]
TERMINATE_COMMAND = ["cmg-cli", "set", "--idle"]
WHEEL_ON_COMMAND = ["cmg-cli", "set", "--wheel"]

KEYS_TO_CHECK_THRESHOLD = [
    "POWER_TMP2",
    "POWER_TMP3",
    "THERMO_X_TEMP",
    "THERMO_Z_TEMP",
    "THERMO_SP_TEMP",
    "THERMO_NSP_TEMP",
]
KEYS_TO_CHECK_STEADY = [
    "THERMO_X_TEMP",
    "THERMO_Z_TEMP",
    "THERMO_SP_TEMP",
    "THERMO_NSP_TEMP",
]


class Parser:
    def __init__(self, log_path: Optional[str] = None):
        self.log_file = open(log_path, "w") if log_path else None
        self.columns = None

    def timestamp_to_seconds(self, ts: str) -> float:
        # Format: YEAR-MONTH-DAYTHOUR:MINUTE:SECOND.MICROSECOND
        date, time = ts.split("T")
        year, month, day = map(int, date.split("-"))
        hour, minute, second = map(float, time.split(":"))
        second, microsecond = map(int, str(second).split("."))
        total_seconds = (
            (((year * 365 + month * 30 + day) * 24 + hour) * 60 + minute) * 60
            + second
            + microsecond / 1_000_000
        )
        return total_seconds

    def parse_json(self, data: dict) -> dict:
        row = {}
        row["TIME"] = self.timestamp_to_seconds(data["TIMESTAMP"])

        power = data["POWER"]
        for key, value in power.items():
            row[f"POWER_{key}"] = value

        thermo = data["THERMOCOUPLE"]
        for pos, data in thermo.items():
            for key, value in data.items():
                row[f"THERMO_{pos}_{key}"] = value

        return row

    def parse(self, line: str, log: bool = False) -> dict:
        data = json.loads(line)
        row = self.parse_json(data)

        if self.log_file and log:
            if self.columns is None:
                self.columns = list(row.keys())
                self.log_file.write(",".join(self.columns) + "\n")

            self.log_file.write(",".join(str(row[col]) for col in self.columns) + "\n")

        return row


class DeviceManager:
    def __init__(
        self,
        parser: Parser,
        threshold: float,
        time_limit: int,
        steady_window: Optional[int] = None,
        steady_threshold: Optional[float] = None,
        check_steady_every: Optional[int] = None,
    ):
        self.terminated = False
        atexit.register(self.terminate)

        self.parser = parser

        self.threshold = threshold

        self.time_limit = time_limit
        self.start_time = None

        self.steady_window = steady_window
        self.steady_threshold = steady_threshold
        self.check_steady_every = check_steady_every
        self.steady_history = {key: deque() for key in KEYS_TO_CHECK_STEADY}
        self.last_steady_check = None

    def read(self) -> Generator[dict, None, None]:
        proc = subprocess.Popen(
            READ_COMMAND, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True
        )

        if select.select([proc.stderr], [], [], 0)[0]:
            err_msgs = []
            for line in proc.stderr:
                err_msgs.append(line.strip())
            raise RuntimeError("Failed to start thermo-cli: \n" + "\n".join(err_msgs))

        for line in proc.stdout:
            row = self.parser.parse(line, log=True)

            pass_threshold = self.check_threshold(
                row, self.threshold, KEYS_TO_CHECK_THRESHOLD
            )
            if not pass_threshold:
                print(
                    f"Temperature threshold of {self.threshold}°C exceeded.\n"
                    + "Stopping the test."
                )
                break

            pass_time_limit = self.check_time_limit(row["TIME"])
            if not pass_time_limit:
                print(
                    f"Time limit of {self.time_limit} seconds reached.\n"
                    + "Stopping the test."
                )
                break

            is_steady = self.check_steady(row)
            if is_steady:
                print(
                    f"Device readings have been steady for the last {self.steady_window} seconds.\n"
                    + "Stopping the test."
                )
                break
            
            yield row

    def wheel_on(self, speed: float, gimbal: float = 45):
        print(
            f"Turning on the wheel with speed {speed} Hz and gimbal angle {gimbal} degrees..."
        )
        command = WHEEL_ON_COMMAND + [f"{speed},{gimbal}"]
        subprocess.run(command, check=True)

    def check_threshold(self, row: dict, threshold: float, keys: list[str]) -> bool:
        for key in keys:
            if row.get(key, 0) >= threshold:
                return False
        return True

    def check_time_limit(self, current_time: float) -> bool:
        if self.start_time is None:
            self.start_time = current_time
            return True
        elapsed_time = current_time - self.start_time
        return elapsed_time <= self.time_limit

    def check_steady(self, row: dict) -> bool:
        if (
            self.steady_window is None
            or self.steady_threshold is None
            or self.check_steady_every is None
        ):
            return True
        
        steady = {key: False for key in KEYS_TO_CHECK_STEADY}
        time = row["TIME"]

        for key in KEYS_TO_CHECK_STEADY:
            history = self.steady_history[key]
            temp = row[key]
            time = row["TIME"]

            history.append((time, temp))  # (time, value)
            
            last_entry = history[-1]
            if (
                (self.last_steady_check is not None and time - self.last_steady_check < self.check_steady_every) or
                last_entry[0] - history[0][0] < self.check_steady_every
            ):
                continue
            
            data = [t for _, t in history]
            std = np.std(data)
            if std < self.steady_threshold:
                steady[key] = True

            self.last_steady_check = time
            while last_entry[0] - history[0][0] >= self.steady_window:
                history.popleft()

        return all(steady.values())

    def terminate(self):
        while not self.terminated:
            print("Terminating device...")
            term_proc = subprocess.Popen(
                TERMINATE_COMMAND,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
            )
            _, _ = term_proc.communicate()
            if term_proc.returncode == 0:
                self.terminated = True

        if self.terminated:
            print("Device terminated.")


def main(
    output: str = typer.Argument(..., help="Output CSV file path"),
    speed: Optional[float] = typer.Option(
        None, help="Wheel speed to turn on the device (Hz)"
    ),
    gimbal: Optional[float] = typer.Option(
        None, help="Gimbal angle to set when turning on the wheel (degrees)"
    ),
    threshold: float = typer.Option(
        70.0, help="Temperature threshold to stop the test (°C)"
    ),
    time_limit: int = typer.Option(
        3600, help="Time limit for the steady test in seconds"
    ),
    steady_window: Optional[int] = typer.Option(
        None, help="Time window to check for steadiness in seconds"
    ),
    steady_threshold: Optional[float] = typer.Option(
        None, help="Maximum allowed variation in temperature for steadiness (°C)"
    ),
    check_steady_every: Optional[int] = typer.Option(
        None, help="Interval to check for steadiness in seconds"
    ),
):
    parser = Parser(output)
    manager = DeviceManager(
        parser=parser,
        threshold=threshold,
        time_limit=time_limit,
        steady_window=steady_window,
        steady_threshold=steady_threshold,
        check_steady_every=check_steady_every,
    )

    try:
        if speed is not None or gimbal is not None:
            assert (
                speed is not None and gimbal is not None
            ), "Both speed and gimbal must be provided."
            manager.wheel_on(speed=speed, gimbal=gimbal)

        reader = manager.read()
        for row in reader:
            print(json.dumps(row, indent=2))

    except Exception as e:
        print(e)


if __name__ == "__main__":
    typer.run(main)
