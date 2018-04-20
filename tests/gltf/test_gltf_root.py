import pytest
import json
from panda3d import core

# Skip these tests if we can't import panda3d.gltf.
gltf = pytest.importorskip("panda3d.gltf")


def test_root_gc():
    root = gltf.Root()
    mat = root.make_material("material")
    assert not root.garbage_collect()

    mat = None
    assert root.garbage_collect()
    assert len(root.materials) == 0
