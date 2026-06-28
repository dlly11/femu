"""Unit tests for femu.build command construction.

These mock out the subprocess layer (run_command) so nothing is actually built;
they assert that the right cmake invocations are constructed.
"""

from __future__ import annotations

import subprocess
from pathlib import Path
from unittest.mock import MagicMock

import pytest
from femu import build


@pytest.fixture
def fake_build_dir(tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> Path:
    bd = tmp_path / "build"
    bd.mkdir()
    monkeypatch.setattr(build, "BUILD_DIR", bd)
    return bd


@pytest.fixture
def mock_run(monkeypatch: pytest.MonkeyPatch) -> MagicMock:
    m = MagicMock(return_value=subprocess.CompletedProcess([], 0))
    monkeypatch.setattr(build, "run_command", m)
    return m


class TestConfigure:
    def test_default_debug_with_sanitizers(self, fake_build_dir: Path, mock_run: MagicMock) -> None:
        build.configure()
        cmd = mock_run.call_args.args[0]
        assert "cmake" in cmd
        assert "-DCMAKE_BUILD_TYPE=Debug" in cmd
        assert "-DENABLE_SANITIZERS=ON" in cmd
        assert "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON" in cmd

    def test_release_without_sanitizers(self, fake_build_dir: Path, mock_run: MagicMock) -> None:
        build.configure(build_type="Release", sanitizers=False)
        cmd = mock_run.call_args.args[0]
        assert "-DCMAKE_BUILD_TYPE=Release" in cmd
        assert "-DENABLE_SANITIZERS=OFF" in cmd

    def test_clang_compiler_sets_env(self, fake_build_dir: Path, mock_run: MagicMock) -> None:
        build.configure(compiler="clang")
        env = mock_run.call_args.kwargs["env"]
        assert env["CC"] == "clang"
        assert env["CXX"] == "clang++"

    def test_no_compiler_passes_no_env(self, fake_build_dir: Path, mock_run: MagicMock) -> None:
        build.configure()
        assert mock_run.call_args.kwargs["env"] is None


class TestCompile:
    def test_compile_builds_with_jobs(self, fake_build_dir: Path, mock_run: MagicMock) -> None:
        build.compile_project(jobs=4)
        cmd = mock_run.call_args.args[0]
        assert cmd[:3] == ["cmake", "--build", str(fake_build_dir)]
        assert "-j" in cmd and "4" in cmd

    def test_compile_with_target(self, fake_build_dir: Path, mock_run: MagicMock) -> None:
        build.compile_project(jobs=1, target="armv8m_emulator")
        cmd = mock_run.call_args.args[0]
        assert "--target" in cmd
        assert "armv8m_emulator" in cmd


class TestRunAnalysis:
    def test_cppcheck_target(self, fake_build_dir: Path, mock_run: MagicMock) -> None:
        assert build.run_analysis(tool="cppcheck") is True
        cmd = mock_run.call_args.args[0]
        assert cmd == ["cmake", "--build", str(fake_build_dir), "--target", "cppcheck"]

    def test_default_target_is_analyze(self, fake_build_dir: Path, mock_run: MagicMock) -> None:
        build.run_analysis()
        cmd = mock_run.call_args.args[0]
        assert cmd[-1] == "analyze"

    def test_returns_false_on_failure(
        self, fake_build_dir: Path, monkeypatch: pytest.MonkeyPatch
    ) -> None:
        def boom(*_args: object, **_kwargs: object) -> None:
            raise subprocess.CalledProcessError(1, "cmake")

        monkeypatch.setattr(build, "run_command", boom)
        assert build.run_analysis(tool="clang-tidy") is False
