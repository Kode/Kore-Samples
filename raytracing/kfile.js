const project = new Project('raytracing');

await project.addProject(findKore());

project.addFile('sources/**');
project.addKongDir('shaders');
project.setDebugDir('deployment');

/*project.addIncludeDir('../nvapi');
project.addLib('../nvapi/amd64/nvapi64');
project.addDefine('KOPE_NVAPI');*/

if (Options.screenshot) {
	project.addDefine('SCREENSHOT');
	project.addDefine('KORE_D3D12_FORCE_WARP');
}

project.flatten();

resolve(project);
