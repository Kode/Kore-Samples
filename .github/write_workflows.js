const fs = require('fs');
const path = require('path');

const samples = [
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
  '15_deinterleaved_buffers',
  'shader',
  'texture',
  //'multiwindow',
  'computeshader',
  'texturearray',
  //'runtime_shader_compilation',
  'raytracing',
  'bindless',
  'computeshader_cpu',
  'computeshader_async'
];

const workflowsDir = path.join('workflows');

function writeWorkflow(workflow) {
  if (!workflow.active) {
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

  const workflowName = workflow.gfx ? (workflow.sys + ' (' + workflow.gfx + ')') : workflow.sys;
  let workflowText = `name: ${workflowName}

on:
  push:
    branches:
    - v3
  pull_request:
    branches:
    - v3

jobs:
  build:

    runs-on: ${workflow.runsOn}

    steps:
    - uses: actions/checkout@v4
${java}
${steps}
`;

if (workflow.sys === 'Windows' && workflow.gfx === 'Direct3D 12') {
    workflowText +=
`    - name: Install WARP 1.0.13
      run: nuget install Microsoft.Direct3D.WARP
`;
}

workflowText +=
`    - name: Get Submodules
      run: ./get_dlc
${postfixSteps}
`;

  for (let index = 0; index < samples.length; ++index) {
    if (!workflow.checked[index]) {
      continue;
    }

    const sample = samples[index];

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
    let sys = null;
    switch (workflow.sys) {
    case 'macOS':
      sys = 'osx';
      break;
    case 'UWP':
      sys = 'windowsapp';
      break;
    case 'Web Assembly':
      sys = 'wasm';
      break;
    default:
      sys = workflow.sys.toLowerCase();
      break;
    }
    const vs = workflow.vs ? ' -v ' + workflow.vs : '';

    if (workflow.sys === 'Windows' && workflow.gfx === 'Direct3D 12') {
      workflowText +=
`    - name: Copy WARP
      working-directory: ${sample}
      run: echo F|xcopy D:\\a\\Kore-Samples\\Kore-Samples\\Microsoft.Direct3D.WARP.1.0.13\\build\\native\\bin\\x64\\d3d10warp.dll deployment\\d3d10warp.dll
`;
    }

    workflowText +=
`    - name: Compile ${sample}
      working-directory: ${sample}
      run: ${prefix}../kore/make ${sys}${vs}${gfx}${options} --debug`;

    if (workflow.canExecute) {
      if (workflow.xvfb) {
        workflowText += ' --option screenshot --compile\n';
        workflowText +=
`    - name: Run ${sample}
      working-directory: ${sample}/deployment
      run: xvfb-run ../build/debug/${sample}`;
      }
      else {
        if (workflow.noCompile) {
          workflowText += ' --option screenshot';
        }
        else{
          workflowText += ' --option screenshot --run';
        }
      }
    }
    else {
      if (!workflow.noCompile) {
        workflowText += ' --compile';
      }
    }

    workflowText += postfix + '\n';

    if (workflow.emscriptenScreenshot) {
      workflowText +=
`    - name: Run ${sample}
      working-directory: ${sample}
      run: node ../.github/emscripten_screenshot.js
`;
    }

    if (workflow.env) {
      workflowText += workflow.env;
    }

    if (workflow.canExecute) {
      workflowText +=
`    - name: Check ${sample}
      working-directory: ${sample}
      run: node ../.github/compare.js
    - name: Upload ${sample} failure image
      if: failure()
      uses: actions/upload-artifact@v4
      with:
        name: ${sample} image
        path: ${sample}/deployment/test.png
`;
    }
  }

  let name = workflow.sys.toLowerCase().replace(/ /g, '');
  if (workflow.gfx) {
    name += '-' + workflow.gfx.toLowerCase().replace(/ /g, '');
  }
  fs.writeFileSync(path.join(workflowsDir, name + '.yml'), workflowText, {encoding: 'utf8'});
}

const workflows = [
  {
    sys: 'Android',
    gfx: 'OpenGL',
    active: true,
    runsOn: 'ubuntu-latest',
    java: true,
    checked: [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0]
  },
  {
    sys: 'Android',
    gfx: 'Vulkan',
    active: true,
    runsOn: 'ubuntu-latest',
    java: true,
    canExecute: false,
    checked: [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0]
  },
  {
    sys: 'Emscripten',
    gfx: 'WebGL',
    active: true,
    runsOn: 'ubuntu-latest',
    checked: [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0],
    steps: '',
    compilePrefix: '../emsdk/emsdk activate latest && source ../emsdk/emsdk_env.sh && ',
    compilePostfix: ' && cd build/debug && make',
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
    active: true,
    runsOn: 'windows-latest',
    canExecute: true,
    checked: [1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0],
    noCompile: true,
    compilePrefix: '../emsdk/emsdk activate latest && ../emsdk/emsdk_env.bat && ',
    compilePostfix: ' && cd build/debug && make',
    emscriptenScreenshot: true,
    postfixSteps:
`    - name: Setup emscripten
      run: git clone https://github.com/emscripten-core/emsdk.git && cd emsdk && ./emsdk install latest
    - name: Setup puppeteer
      working-directory: .github
      run: npm install puppeteer node-static @actions/core
`
  },
  {
    sys: 'Web Assembly',
    gfx: 'WebGL',
    active: true,
    runsOn: 'ubuntu-latest',
    steps: '',
    noCompute: true,
    noTexArray: true,
    checked: [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0],
    steps:
`    - name: Apt Update
      run: sudo apt update
    - name: Apt Install
      run: sudo apt install ninja-build
`
  },
  {
    sys: 'Web Assembly',
    gfx: 'WebGPU',
    runsOn: 'ubuntu-latest',
    canExecute: false,
    checked: [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0],
    steps:
`    - name: Apt Update
      run: sudo apt update
    - name: Apt Install
      run: sudo apt install ninja-build
`
  },
  {
    sys: 'iOS',
    gfx: 'Metal',
    active: true,
    runsOn: 'macOS-latest',
    options: '--nosigning',
    canExecute: false,
    checked: [1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,1,1,1,1,0,0,0,0]
  },
  {
    sys: 'iOS',
    gfx: 'OpenGL',
    active: true,
    runsOn: 'macOS-latest',
    options: '--nosigning',
    noCompute: true,
    checked: [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0]
  },
  {
    sys: 'Linux',
    gfx: 'OpenGL',
    cpu: 'ARM',
    active: true,
    checked: [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0]
  },
  {
    sys: 'Linux',
    gfx: 'OpenGL',
    active: true,
    runsOn: 'ubuntu-latest',
    canExecute: true,
    xvfb: true,
    steps:
`    - name: Apt Update
      run: sudo apt update
    - name: Apt Install
      run: sudo apt-get install libasound2-dev libxinerama-dev libxrandr-dev libgl1-mesa-dev libxi-dev libxcursor-dev libudev-dev libwayland-dev wayland-protocols libxkbcommon-dev ninja-build imagemagick xvfb --yes --quiet
`,
    RuntimeShaderCompilation: true,
    checked: [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0]
  },
  {
    sys: 'Linux',
    gfx: 'Vulkan',
    active: true,
    runsOn: 'ubuntu-latest',
    canExecute: true,
    xvfb: true,
    checked: [1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0],
    steps:
`    - name: Get LunarG key
      run: wget -qO- https://packages.lunarg.com/lunarg-signing-key-pub.asc | sudo tee /etc/apt/trusted.gpg.d/lunarg.asc
    - name: Get LunarG apt sources
      run: sudo wget -qO /etc/apt/sources.list.d/lunarg-vulkan-noble.list http://packages.lunarg.com/vulkan/lunarg-vulkan-noble.list
    - name: Apt Update
      run: sudo apt update
    - name: Apt Install
      run: sudo apt install libasound2-dev libxinerama-dev libxrandr-dev libgl1-mesa-dev libxi-dev libxcursor-dev libudev-dev vulkan-sdk mesa-vulkan-drivers libvulkan1 vulkan-tools vulkan-validationlayers libwayland-dev wayland-protocols libxkbcommon-dev ninja-build imagemagick xvfb --yes --quiet
`
  },
  {
    sys: 'macOS',
    gfx: 'Metal',
    active: true,
    runsOn: '[self-hosted, macOS]',
    canExecute: true,
    checked: [1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,1,1,1,1,0,0,0,0]
  },
  {
    sys: 'macOS',
    gfx: 'OpenGL',
    active: true,
    runsOn: 'macOS-latest',
    checked: [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0]
  },
  {
    sys: 'UWP',
    active: true,
    runsOn: 'windows-latest',
    vs: 'vs2022',
    checked: [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0]
  },
  {
    sys: 'Windows',
    gfx: 'Direct3D 11',
    active: true,
    runsOn: 'windows-latest',
    RuntimeShaderCompilation: true,
    vs: 'vs2022',
    checked: [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0]
  },
  {
    sys: 'Windows',
    gfx: 'Direct3D 12',
    active: true,
    runsOn: 'windows-latest',
    canExecute: true,
    vs: 'vs2022',
    checked: [1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0]
  },
  {
    sys: 'Windows',
    gfx: 'OpenGL',
    active: true,
    runsOn: 'windows-latest',
    canExecute: false,
    vs: 'vs2022',
    checked: [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0]
  },
  {
    sys: 'Windows',
    gfx: 'Vulkan',
    active: true,
    runsOn: 'windows-latest',
    canExecute: false,
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
          $installer.WaitForExit();`,
    checked: [1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0]
  }
];

for (const workflow of workflows) {
  writeWorkflow(workflow);
}
