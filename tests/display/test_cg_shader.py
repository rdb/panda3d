import os
import platform
import pytest
from _pytest.outcomes import Failed

from panda3d import core


SHADERS_DIR = core.Filename.from_os_specific(os.path.dirname(__file__))


# This is the template for the compute shader that is used by run_cg_test.
# It's actually written in a hodgepodge of Cg and HLSL, since Cg does not
# actually support compute shaders or image load/store.
# It defines an assert() macro that writes failures to a buffer, indexed by
# line number.
# The reset() function serves to prevent the _triggered variable from being
# optimized out in the case that the assertions are being optimized out.
CG_VERTEX_TEMPLATE = """//Cg profile hlslc

{preamble}

RWBuffer<uint> _triggered;

void _reset() {{
    _triggered[0] = 0;
    AllMemoryBarrier();
}}

void _assert(bool cond, int lineno) {{
    if (!cond) _triggered[lineno] = 16;
}}

void _assert(bool2 cond, int lineno) {{
    if (!all(cond)) _triggered[lineno] = !cond[0] | (!cond[1] << 1);
}}

void _assert(bool3 cond, int lineno) {{
    if (!all(cond)) _triggered[lineno] = !cond[0] | (!cond[1] << 1) | (!cond[2] << 2);
}}

void _assert(bool4 cond, int lineno) {{
    if (!all(cond)) _triggered[lineno] = !cond[0] | (!cond[1] << 1) | (!cond[2] << 2) | (!cond[3] << 3);
}}

#define assert(cond) _assert(cond, __LINE__)

void cshader() {{
    _reset();
{body}
}}
"""


def run_cg_test(gsg, body, preamble="", inputs={},
                state=core.RenderState.make_empty()):
    """ Runs a Cg test on the given GSG.  The given body is executed in the
    main function and should call assert().  The preamble should contain all
    of the shader inputs. """

    if not gsg.supports_basic_shaders:
        pytest.skip("basic shaders not supported")

    if not gsg.supports_compute_shaders:
        pytest.skip("compute shaders not supported")

    if not gsg.supports_buffer_texture:
        pytest.skip("buffer textures not supported")

    __tracebackhide__ = True

    preamble = preamble.strip()
    body = body.rstrip().lstrip('\n')
    code = CG_VERTEX_TEMPLATE.format(preamble=preamble, body=body)
    line_offset = code[:code.find(body)].count('\n') + 1
    shader = core.Shader.make_compute(core.Shader.SL_Cg, code)
    if not shader:
        pytest.fail("error compiling shader:\n" + code)

    # Create a buffer to hold the results of the assertion.  We use one byte
    # per line of shader code, so we can show which lines triggered.
    result = core.Texture("")
    result.set_clear_color((0, 0, 0, 0))
    result.setup_buffer_texture(code.count('\n'), core.Texture.T_unsigned_byte,
                                core.Texture.F_r8i, core.GeomEnums.UH_static)

    # Build up the shader inputs
    attrib = core.ShaderAttrib.make(shader)
    for name, value in inputs.items():
        attrib = attrib.set_shader_input(name, value)
    attrib = attrib.set_shader_input('_triggered', result)
    state = state.set_attrib(attrib)

    # Run the compute shader.
    engine = core.GraphicsEngine.get_global_ptr()
    try:
        engine.dispatch_compute((1, 1, 1), state, gsg)
    except AssertionError as exc:
        assert False, "Error executing compute shader:\n" + code

    # Download the texture to check whether the assertion triggered.
    assert engine.extract_texture_data(result, gsg)
    triggered = result.get_ram_image()
    if any(triggered):
        count = len(triggered) - triggered.count(0)
        lines = body.split('\n')
        formatted = ''
        for i, line in enumerate(lines):
            mask = triggered[i + line_offset]
            if mask:
                print(mask)
                formatted += '=>  ' + line
                components = ''
                if mask & 1:
                    components += 'x'
                if mask & 2:
                    components += 'y'
                if mask & 4:
                    components += 'z'
                if mask & 8:
                    components += 'w'
                formatted += f'      <= {components} components don\'t match'
            else:
                formatted += '    ' + line
            formatted += '\n'
        pytest.fail("{0} Cg assertions triggered:\n{1}".format(count, formatted))


def run_cg_compile_check(gsg, shader_path, expect_fail=False):
    """Compile supplied Cg shader path and check for errors"""
    shader = core.Shader.load(shader_path, core.Shader.SL_Cg)
    # assert shader.is_prepared(gsg.prepared_objects)
    if expect_fail:
        assert shader is None
    else:
        assert shader is not None


def test_cg_compile_error(gsg):
    """Test getting compile errors from bad Cg shaders"""
    shader_path = core.Filename(SHADERS_DIR, 'cg_bad.sha')
    run_cg_compile_check(gsg, shader_path, expect_fail=True)


def test_cg_from_file(gsg):
    """Test compiling Cg shaders from files"""
    shader_path = core.Filename(SHADERS_DIR, 'cg_simple.sha')
    run_cg_compile_check(gsg, shader_path)


def test_cg_test(gsg):
    "Test to make sure that the Cg tests work correctly."

    run_cg_test(gsg, "assert(true);")


def test_cg_test_fail(gsg):
    "Same as above, but making sure that the failure case works correctly."

    with pytest.raises(Failed):
        run_cg_test(gsg, "assert(false);")


def test_cg_int(gsg):
    inputs = dict(
        zero=0,
        intmax=0x7fffffff,
        intmin=-0x7fffffff,
    )
    preamble = """
    uniform int zero;
    uniform int intmax;
    uniform int intmin;
    """
    code = """
    assert(zero == 0);
    assert(intmax == 0x7fffffff);
    assert(intmin == -0x7fffffff);
    """
    run_cg_test(gsg, code, preamble, inputs)


def test_cg_state_material(gsg):
    mat = core.Material("mat")
    mat.ambient = (1, 2, 3, 4)
    mat.diffuse = (5, 6, 7, 8)
    mat.emission = (9, 10, 11, 12)
    mat.specular = (13, 14, 15, 0)
    mat.shininess = 16

    preamble = """
    uniform float4x4 attr_material;
    """
    code = """
    assert(attr_material[0] == float4(1, 2, 3, 4));
    assert(attr_material[1] == float4(5, 6, 7, 8));
    assert(attr_material[2] == float4(9, 10, 11, 12));
    assert(attr_material[3].rgb == float3(13, 14, 15));
    assert(attr_material[3].w == 16);
    """

    node = core.NodePath("state")
    node.set_material(mat)

    run_cg_test(gsg, code, preamble, state=node.get_state())


def test_cg_state_fog(gsg):
    fog = core.Fog("fog")
    fog.color = (1, 2, 3, 4)
    fog.exp_density = 0.5
    fog.set_linear_range(6, 10)

    preamble = """
    uniform float4 attr_fog;
    uniform float4 attr_fogcolor;
    """
    code = """
    assert(attr_fogcolor == float4(1, 2, 3, 4));
    assert(attr_fog[0] == 0.5);
    assert(attr_fog[1] == 6);
    assert(attr_fog[2] == 10);
    assert(attr_fog[3] == 0.25);
    """

    node = core.NodePath("state")
    node.set_fog(fog)

    run_cg_test(gsg, code, preamble, state=node.get_state())


def test_cg_texpad_texpix(gsg):
    tex = core.Texture("test")
    tex.setup_2d_texture(16, 32, core.Texture.T_unsigned_byte, core.Texture.F_rgba)
    tex.auto_texture_scale = core.ATS_pad
    tex.set_size_padded(10, 30)

    preamble = """
    uniform float3 texpad_test;
    uniform float2 texpix_test;
    """
    code = """
    assert(texpad_test == float3(10 * 0.5 / 16, 30 * 0.5 / 32, 0.5));
    assert(texpix_test == float2(1.0 / 16, 1.0 / 32));
    """

    run_cg_test(gsg, code, preamble, inputs={"test": tex})


def test_cg_alight(gsg):
    alight = core.AmbientLight("alight")
    alight.set_color((1, 2, 3, 4))
    np = core.NodePath(alight)

    preamble = """
    uniform float4 alight_test;
    """
    code = """
    assert(alight_test == float4(1, 2, 3, 4));
    """

    run_cg_test(gsg, code, preamble, inputs={"test": np})


def test_cg_satten(gsg):
    spot = core.Spotlight("spot")
    spot.set_attenuation((1, 2, 3))
    spot.set_exponent(4)
    np = core.NodePath(spot)

    preamble = """
    uniform float4 satten_test;
    """
    code = """
    assert(satten_test == float4(1, 2, 3, 4));
    """

    run_cg_test(gsg, code, preamble, inputs={"test": np})