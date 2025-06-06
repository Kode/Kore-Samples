const puppeteer = require("puppeteer");
const nstatic = require('node-static');
const core = require('@actions/core');

const fs = require('fs');
const path = require('path');

core.info('Running server on 8888');
const fileServer = new nstatic.Server(path.join(process.cwd(), 'build', 'debug'), { cache: 0 });

const server = require('http').createServer((request, response) => {
	request.addListener('end', function () {
		fileServer.serve(request, response);
	}).resume();
});

server.on('error', (e) => {
	if (e.code === 'EADDRINUSE') {
		core.info('Error: Port 8888 is already in use.');
	}
});

server.listen(8888);

core.info('Starting browser');

let browser = null;

(async () => {
	try {
		browser = await puppeteer.launch({args: ['--enable-gpu', '--ignore-gpu-blocklist']});
		core.info('Browser version: ' + await browser.version());

		const page = await browser.newPage();

		page.on('console', msg => core.info('Page: ' + msg.text()));

		await page.goto('http://localhost:8888');

		const client = await page.target().createCDPSession();
		await client.send('Page.setDownloadBehavior', {
			behavior: 'allow',
			downloadPath: path.join(process.cwd(), 'deployment'),
		});
	}
	catch (e) {
		core.info('Error: ' + e);
	}
	finally {
		setTimeout(() => {
			core.info('Closing browser and server');
			if (browser) {
				browser.close();
			}
			server.close();
			setTimeout(() => {
				if (browser && browser.process()) {
					browser.process().kill('SIGINT');
					core.info('Killed the browser');
				}
				core.info('Browser and server closed');
			}, 5000);
		}, 15000);
	}
})();
