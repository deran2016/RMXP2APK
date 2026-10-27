"""Construct the shared Android builder command."""
import sys


def build_command(builder, project, output, data_encoding, ini_encoding, rtp, ruby, application_id, high_refresh=False):
    command = [sys.executable, str(builder), project, output, "--data-encoding", data_encoding, "--ini-encoding", ini_encoding]
    for flag, value in (("--rtp-dir", rtp), ("--ruby", ruby), ("--application-id", application_id.strip())):
        if value:
            command.extend((flag, value))
    if high_refresh:
        command.append("--120fps")
    return command
