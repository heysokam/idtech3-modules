#include "../vis.h"

//..............................
// PVS
//..............................

//..............................
// CM_ClusterPVS
//   Returns the byte data stored at the given cluster id
//   This will contain the vis data stored inside the currently loaded clipMap
//..............................
byte* CM_ClusterPVS(i32 cluster) {
  if (cluster < 0 || cluster >= cm.numClusters || !cm.vised) { return cm.visibility; }
  return cm.visibility + cluster * cm.clusterBytes;
}


//..............................
// Area Portals
//..............................

//..............................
// CM_FloodArea_r
//   Recursively flood through all areas of the currently stored clipMap
//   starting at areaNum, and setting the passed through areas to floodNum
//..............................
static void CM_FloodArea_r(i32 areaNum, i32 floodNum) {
  cArea* area = &cm.areas[areaNum];
  if (area->floodValid == cm.floodValid) {
    if (area->floodNum == floodNum) return;
    err(ERR_DROP, "%s: reflooded", __func__);
  }
  area->floodNum   = floodNum;
  area->floodValid = cm.floodValid;
  i32* con         = cm.areaPortals + areaNum * cm.numAreas;
  for (i32 areaId = 0; areaId < cm.numAreas; areaId++) {
    if (con[areaId] > 0) { CM_FloodArea_r(areaId, floodNum); }
  }
}

//..............................
// CM_FloodAreaConnections
//   Floods through all connected areas of the currently stored clipMap
//   Starts counting from floodNum 0
//   Increases floodValid by one, and sets this number to all passed through areas
//..............................
void CM_FloodAreaConnections(void) {
  // all current floods are now invalid
  cm.floodValid++;
  i32 floodNum = 0;

  for (i32 areaId = 0; areaId < cm.numAreas; areaId++) {
    cArea* area = &cm.areas[areaId];
    if (area->floodValid == cm.floodValid) { continue; }  // already flooded into
    floodNum++;
    CM_FloodArea_r(areaId, floodNum);
  }
}
//..............................
// CM_AdjustAreaPortalState
//   Sets the state of the connection between two areaId numbers to true/false
//   Refloods the whole map after it
//..............................
void CM_AdjustAreaPortalState(i32 area1, i32 area2, bool open) {
  if (area1 < 0 || area2 < 0) { return; }
  if (area1 >= cm.numAreas || area2 >= cm.numAreas) { err(ERR_DROP, "%s: bad area number", __func__); }
  if (open) {
    cm.areaPortals[area1 * cm.numAreas + area2]++;
    cm.areaPortals[area2 * cm.numAreas + area1]++;
  } else {
    cm.areaPortals[area1 * cm.numAreas + area2]--;
    cm.areaPortals[area2 * cm.numAreas + area1]--;
    if (cm.areaPortals[area2 * cm.numAreas + area1] < 0) { err(ERR_DROP, "%s: negative reference count", __func__); }
  }
  CM_FloodAreaConnections();
}

//..............................
// CM_AreasConnected
//   Return whether two areas are connected or not in the currently loaded clipMap
//   Will always return true, if RAD_DO_VIS is disabled
//   Both areaId numbers should be positive, and within 0..numAreas range
//   If the floodNum of both areas is the same, they are visible
//..............................
bool CM_AreasConnected(i32 area1, i32 area2) {
  if (!col.doVIS) { return true; }
  if (area1 < 0 || area2 < 0) { return false; }
  if (area1 >= cm.numAreas || area2 >= cm.numAreas) { err(ERR_DROP, "area >= cm.numAreas"); }
  if (cm.areas[area1].floodNum == cm.areas[area2].floodNum) { return true; }
  return false;
}
//..............................
// CM_WriteAreaBits
//   Writes a bit buffer of all the areas in the same flood as the given areaId
//   Returns the number of bytes needed to hold all the bits.

//   The bits are OR'd in,
//   so CM_WriteAreaBits can be called from multiple viewpoints 
//   to get the union of all visible areas.
//   Used to cull non-visible entities from snapshots
//..............................
i32 CM_WriteAreaBits(byte* buffer, i32 area) {
  i32 bytes = (cm.numAreas + 7) >> 3;
  if (!col.doVIS || area == -1) {  // for debugging, send everything
    memset(buffer, 255, bytes);
  } else {
    i32 floodNum = cm.areas[area].floodNum;
    for (i32 i = 0; i < cm.numAreas; i++) {
      if (cm.areas[i].floodNum == floodNum || area == -1) buffer[i >> 3] |= 1 << (i & 7);
    }
  }
  return bytes;
}
