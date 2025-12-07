const fs = require('fs');
const path = require('path');

const samples = [
	'Shader',
	'Texture',
	'MultiWindow',
	'ComputeShader',
	'TextureArray',
	'ShaderG5',
  'RuntimeShaderCompilation',
  '00_empty',
  '01_triangle',
  '02_matrix',
  '03_colored_cube',
  '04_textured_cube',
  '05_camera_controls',
  '06_render_targets',
  '07_multiple_render_targets',
  '08_float_render_targets',
  '09_depth_render_targets',
  '10_cubemap',
  '11_instanced_rendering',
  '12_set_render_target_depth',
  '13_generate_mipmaps',
  '14_set_mipmap',
  '15_deinterleaved_buffers'
];

const workflowsDir = path.join('workflows');

function writeWorkflow(workflow) {
  if (workflow.sys === 'FreeBSD') {
    writeFreeBSDWorkflow(workflow);
    return;
  }

  const java = workflow.java
?
`
    - uses: actions/setup-java@v3
      with:
        distribution: 'oracle'
        java-version: '17'
`
:
'';

  const steps = workflow.steps ?? '';
  const postfixSteps = workflow.postfixSteps ?? '';

  let workflowName = workflow.sys;
  if (workflow.cpu) workflowName += ' on ' + workflow.cpu;
  if (workflow.gfx) workflowName += ' (' + workflow.gfx + ')';
  let workflowText = `name: ${workflowName}

on:
  push:
    branches:
    - v2
  pull_request:
    branches:
    - v2

jobs:
  build:

    runs-on: ${workflow.runsOn}

    steps:
    - uses: actions/checkout@v3
${java}
${steps}
    - name: Get Submodules
      run: ./get_dlc
${postfixSteps}
`;

  for (const sample of samples) {
    if (sample === 'RuntimeShaderCompilation') {
      if (!workflow.RuntimeShaderCompilation) {
        continue;
      }
    }

    if (workflow.noCompute && sample === 'ComputeShader') {
      continue;
    }

    if (workflow.noTexArray && sample === 'TextureArray') {
      continue;
    }

    const prefix = workflow.compilePrefix ?? '';
    const postfix = workflow.compilePostfix ?? '';
    const gfx = workflow.gfx ? ((workflow.gfx === 'WebGL') ? ' -g opengl' : ' -g ' + workflow.gfx.toLowerCase().replace(/ /g, '')) : '';
    const options = workflow.options ? ' ' + workflow.options : '';
    const sys = workflow.sys === 'macOS' ? 'osx' : (workflow.sys === 'UWP' ? 'windowsapp' : workflow.sys.toLowerCase());
    const vs = workflow.vs ? ' -v ' + workflow.vs : '';

    workflowText +=
`    - name: Compile ${sample}
      working-directory: ${sample}
      run: ${prefix}../Kore/make ${sys}${vs}${gfx}${options} --compile${postfix}
`;
    if (workflow.env) {
      workflowText += workflow.env;
    }
  }

  let name = workflow.sys.toLowerCase();
  if (workflow.cpu) name += '-' + workflow.cpu.toLowerCase().replace(/ /g, '');
  if (workflow.gfx) name += '-' + workflow.gfx.toLowerCase().replace(/ /g, '');
  fs.writeFileSync(path.join(workflowsDir, name + '.yml'), workflowText, {encoding: 'utf8'});
}

const workflows = [
  {
    sys: 'Android',
    gfx: 'OpenGL',
    runsOn: 'ubuntu-latest',
    java: true
  },
  {
    sys: 'Android',
    gfx: 'Vulkan',
    runsOn: 'ubuntu-latest',
    java: true
  },
  {
    sys: 'Emscripten',
    gfx: 'WebGL',
    runsOn: 'ubuntu-latest',
    steps: '',
    compilePrefix: '../emsdk/emsdk activate latest && source ../emsdk/emsdk_env.sh && ',
    compilePostfix: ' && cd build/release && make',
    postfixSteps:
`    - name: Setup emscripten
      run: git clone https://github.com/emscripten-core/emsdk.git && cd emsdk && ./emsdk install latest
`,
    noCompute: true,
    noTexArray: true
  },
  {
    sys: 'Emscripten',
    gfx: 'WebGPU',
    runsOn: 'ubuntu-latest',
    steps: '',
    compilePrefix: '../emsdk/emsdk activate 3.1.64 && source ../emsdk/emsdk_env.sh && ',
    compilePostfix: ' && cd build/release && make',
    postfixSteps:
`    - name: Setup emscripten
      run: git clone https://github.com/emscripten-core/emsdk.git && cd emsdk && ./emsdk install 3.1.64
`
  },
  {
    sys: 'iOS',
    gfx: 'Metal',
    runsOn: 'macOS-latest',
    options: '--nosigning'
  },
  {
    sys: 'iOS',
    gfx: 'OpenGL',
    runsOn: 'macOS-latest',
    options: '--nosigning',
    noCompute: true
  },
  {
    sys: 'Linux',
    gfx: 'OpenGL',
    runsOn: 'ubuntu-latest',
    steps:
`    - name: Apt Update
      run: sudo apt update
    - name: Apt Install
      run: sudo apt-get install libasound2-dev libxinerama-dev libxrandr-dev libgl1-mesa-dev libxi-dev libxcursor-dev libudev-dev libwayland-dev wayland-protocols libxkbcommon-dev ninja-build --yes --quiet
`,
    RuntimeShaderCompilation: true
  },
  {
    sys: 'Linux',
    gfx: 'OpenGL',
    cpu: 'ARM',
    runsOn: 'ubuntu-22.04-arm',
    steps:
`    - name: Apt Update
      run: sudo apt update
    - name: Apt Install
      run: sudo apt-get install libasound2-dev libxinerama-dev libxrandr-dev libgl1-mesa-dev libxi-dev libxcursor-dev libudev-dev libwayland-dev wayland-protocols libxkbcommon-dev ninja-build --yes --quiet
`
  },
  {
    sys: 'Linux',
    gfx: 'Vulkan',
    runsOn: 'ubuntu-latest',
    steps:
`    - name: Get LunarG key
      run: wget -qO- https://packages.lunarg.com/lunarg-signing-key-pub.asc | sudo tee /etc/apt/trusted.gpg.d/lunarg.asc
    - name: Get LunarG apt sources
      run: sudo wget -qO /etc/apt/sources.list.d/lunarg-vulkan-noble.list http://packages.lunarg.com/vulkan/lunarg-vulkan-noble.list
    - name: Apt Update
      run: sudo apt update
    - name: Apt Install
      run: sudo apt install libasound2-dev libxinerama-dev libxrandr-dev libgl1-mesa-dev libxi-dev libxcursor-dev libudev-dev vulkan-sdk libwayland-dev wayland-protocols libxkbcommon-dev ninja-build --yes --quiet
`
  },
  {
    sys: 'macOS',
    gfx: 'Metal',
    runsOn: 'macOS-latest',
    RuntimeShaderCompilation: true
  },
  {
    sys: 'macOS',
    gfx: 'OpenGL',
    runsOn: 'macOS-latest'
  },
  {
    sys: 'UWP',
    runsOn: 'windows-latest',
    vs: 'vs2022'
  },
  {
    sys: 'Windows',
    gfx: 'Direct3D 9',
    runsOn: 'windows-latest',
    noCompute: true,
    noTexArray: true,
    vs: 'vs2022',
    postfixSteps:
`    - name: Install DirectX
      run: choco install -y directx --no-progress`
  },
  {
    sys: 'Windows',
    gfx: 'Direct3D 11',
    runsOn: 'windows-latest',
    RuntimeShaderCompilation: true,
    vs: 'vs2022'
  },
  {
    sys: 'Windows',
    gfx: 'Direct3D 12',
    runsOn: 'windows-latest',
    vs: 'vs2022'
  },
  {
    sys: 'Windows',
    gfx: 'OpenGL',
    runsOn: 'windows-latest',
    vs: 'vs2022'
  },
  {
    sys: 'Windows',
    gfx: 'Vulkan',
    runsOn: 'windows-latest',
    vs: 'vs2022',
    env:
`      env:
        VULKAN_SDK: C:\\VulkanSDK\\1.3.275.0
`,
    steps:
`    - name: Setup Vulkan
      run: |
          Invoke-WebRequest -Uri "https://sdk.lunarg.com/sdk/download/1.3.275.0/windows/VulkanSDK-1.3.275.0-Installer.exe" -OutFile VulkanSDK.exe
          $installer = Start-Process -FilePath VulkanSDK.exe -Wait -PassThru -ArgumentList @("--da", "--al", "-c", "in");
          $installer.WaitForExit();`
  }
];

for (const workflow of workflows) {
  writeWorkflow(workflow);
}
