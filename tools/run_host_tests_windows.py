"""Run upstream tests with a Windows temporary-cwd cleanup adapter.

No product or upstream test files are changed. Git integration cases are skipped
explicitly if Git is unavailable; their results must not be reported as passes.
"""
import os, pathlib, tempfile, unittest, shutil
root=pathlib.Path(__file__).resolve().parent.parent/'source/voice'
# Set CXX externally if a non-default host compiler is required.
cleanup=tempfile.TemporaryDirectory.cleanup
def windows_cleanup(self):
    try:
        if pathlib.Path.cwd().is_relative_to(pathlib.Path(self.name)):
            os.chdir(root)
    except (OSError,ValueError): os.chdir(root)
    return cleanup(self)
tempfile.TemporaryDirectory.cleanup=windows_cleanup
suite=unittest.defaultTestLoader.discover(str(root/'scripts/tests'))
def prepare(suite):
    for test in suite:
        if isinstance(test,unittest.TestSuite): prepare(test)
        elif '.WorkflowDiffTests.' in test.id() and (not shutil.which('git') or not shutil.which('bash')):
            method=getattr(test,test._testMethodName)
            setattr(test,test._testMethodName,unittest.skip('Git/Bash integration runtime is unavailable on this Windows host')(method))
prepare(suite)
result=unittest.TextTestRunner(verbosity=2).run(suite)
raise SystemExit(not result.wasSuccessful())
