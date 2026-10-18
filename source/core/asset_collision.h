#ifndef UPRISING_ASSET_COLLISION_H
#define UPRISING_ASSET_COLLISION_H
#include "asset_mesh.h"
#include "resource_blob.h"
/*4ef0e0's transient ASC owner. The returned model owns contour vertices and
 *edge vectors; callers publish them into the existing collision owner. */
int asset_collision_read_asc(const ResourceBlob *,float half,float unit,AssetModel *);
#endif
