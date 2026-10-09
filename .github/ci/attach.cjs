// Downloaded packages are never unpacked/repacked: only the attachment name changes.
module.exports = async ({ github, context, core, packages }) => {
  const fs = require('node:fs');
  const path = require('node:path');
  const crypto = require('node:crypto');
  const tag = context.payload.release.tag_name;
  for (const file of packages) {
    const data = fs.readFileSync(path.join('release-packages', file));
    const name = file.replace(/^opentyrian-/, `opentyrian-${tag}-`);
    core.info(`${name}: SHA-256 ${crypto.createHash('sha256').update(data).digest('hex')}`);
    // Match the previous gh --clobber behavior for retrying a partial release.
    const assets = await github.paginate(github.rest.repos.listReleaseAssets, {
      ...context.repo, release_id: context.payload.release.id, per_page: 100
    });
    const existing = assets.find(asset => asset.name === name);
    if (existing) await github.rest.repos.deleteReleaseAsset({
      ...context.repo, asset_id: existing.id
    });
    await github.rest.repos.uploadReleaseAsset({ ...context.repo,
      release_id: context.payload.release.id, name, data,
      headers: { 'content-type': file.endsWith('.zip') ? 'application/zip' : 'application/gzip' }
    });
  }
};
