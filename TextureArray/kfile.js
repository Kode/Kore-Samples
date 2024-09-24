const project = new Project('TextureArray');

await project.addProject('../Kinc', {kong: true, kope: true});

project.addFile('Sources/**');
project.addKongDir('Shaders');
project.setDebugDir('Deployment');

if (Options.screenshot) {
	project.addDefine('SCREENSHOT');
}

project.flatten();

resolve(project);
