"""Unit tests for the femu CLI wiring (femu.cli).

Driven through click's CliRunner. Commands that would invoke the real build are
mocked; the pure informational commands run as-is.
"""

from __future__ import annotations

import pytest
from click.testing import CliRunner
from femu import build
from femu.cli import main


@pytest.fixture
def runner() -> CliRunner:
    return CliRunner()


class TestTopLevel:
    def test_version(self, runner: CliRunner) -> None:
        result = runner.invoke(main, ["--version"])
        assert result.exit_code == 0
        assert "femu" in result.output

    def test_help_lists_subcommands(self, runner: CliRunner) -> None:
        result = runner.invoke(main, ["--help"])
        assert result.exit_code == 0
        for sub in ("build", "test", "dev", "run"):
            assert sub in result.output


class TestDevCommands:
    def test_dev_list_shows_modules(self, runner: CliRunner) -> None:
        result = runner.invoke(main, ["dev", "list"])
        assert result.exit_code == 0
        assert "decoder" in result.output
        assert "executor" in result.output

    def test_dev_context_points_to_current_docs(self, runner: CliRunner) -> None:
        # Regression guard: the doc reorganization must keep this path valid.
        result = runner.invoke(main, ["dev", "context", "decoder"])
        assert result.exit_code == 0
        assert "docs/architecture/overview.md" in result.output
        assert "docs/ARCHITECTURE.md" not in result.output


class TestBuildAnalyzeWiring:
    def test_analyze_passes_tool_through(
        self, runner: CliRunner, monkeypatch: pytest.MonkeyPatch
    ) -> None:
        calls: list[str | None] = []

        def fake_run_analysis(tool: str | None = None) -> bool:
            calls.append(tool)
            return True

        monkeypatch.setattr(build, "run_analysis", fake_run_analysis)
        result = runner.invoke(main, ["build", "analyze", "--tool", "cppcheck"])
        assert result.exit_code == 0
        assert calls == ["cppcheck"]

    def test_analyze_failure_exits_nonzero(
        self, runner: CliRunner, monkeypatch: pytest.MonkeyPatch
    ) -> None:
        monkeypatch.setattr(build, "run_analysis", lambda tool=None: False)
        result = runner.invoke(main, ["build", "analyze"])
        assert result.exit_code == 1


class TestRunArgValidation:
    def test_run_missing_firmware_errors(self, runner: CliRunner) -> None:
        result = runner.invoke(main, ["run", "/no/such/firmware.elf"])
        # click's Path(exists=True) rejects the missing file
        assert result.exit_code != 0
