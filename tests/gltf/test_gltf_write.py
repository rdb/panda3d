import pytest
import json
from panda3d import core

# Skip these tests if we can't import panda3d.gltf.
gltf = pytest.importorskip("panda3d.gltf")


def serialize(object):
    stream = core.StringStream(b"")
    object.write_json(stream)
    return json.loads(stream.data.decode('utf-8'))


def test_write_empty():
    data = gltf.Root()
    assert serialize(data) == {
        'asset': {
            'version': '2.0',
        },
    }


def test_write_node():
    data = gltf.Node()
    assert serialize(data) == {}

    data.translation = (1, 2, 3)
    assert serialize(data) == {'translation': [1, 2, 3]}
