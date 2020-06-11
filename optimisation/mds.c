#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <assert.h>
#include <time.h>
#include <sys/time.h>
#include <pthread.h>
#include <fcntl.h>
#include <math.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "common.h"
#include "randlib.h"

#define M_PIF ((float) M_PI)


char *executable = NULL;
int reset = 1;
int lastExchange = 0;
int exchanges = 0;
float magicFactor = 0.7f;
int exEnabled = 1;

#define MAX_NODES 20
float trueDists[MAX_NODES][MAX_NODES] = {{0}};
float curDists[MAX_NODES][MAX_NODES] = {{0}};
float R[MAX_NODES] = {0};
float T[MAX_NODES] = {0};
float K = 0;
int ids[MAX_NODES] = {0};
int numNodes = 0;

typedef struct
{
    float x;
    float y;
} MDSPoint;

typedef struct
{
    float error;
    MDSPoint currentPositions[MAX_NODES];
    MDSPoint nextPositions[MAX_NODES];
} FooBar;

FooBar curState;
FooBar altState;

int get_index_by_id (uint32_t id, int createIfNotFound)
{
    assert (id);
    size_t n = sizeof (ids) / sizeof (ids[0]);
    int i;
    for (i=0; i<numNodes && i<n; i++)
        if (ids[i] == id)
            return i;
    if (i == numNodes && i<n && createIfNotFound)
    {
        ids[i] = id;
        numNodes++;
        return i;
    }
    return -1;
}

void init_model (char *modelFile)
{
    FILE *fs = fopen (modelFile, "r");
    if (!fs)
        exit_error ("can't open %s: %s", modelFile, strerror (errno));
    char buf[256];

    if (fgets (buf, sizeof (buf), fs) != buf)
        exit_error ("read error");

    if (sscanf (buf, "%f", &K) != 1)
        exit_error ("scan error");

    while (fgets (buf, sizeof (buf), fs) == buf)
    {
        int id;
        float r, t;

        if (sscanf (buf, "%x %f %f", &id, &r, &t) != 3)
            exit_error ("scan error");

        int idx = get_index_by_id (id, 1);
        R[idx] = r;
        T[idx] = t;
    }
}

float get_dist (int i, int j, float rssi)
{
    float dij = __exp10f ((rssi - R[i] - T[j]) / K);
    catch_nan (dij);
    return dij * 0.01f; // convert to meter
}

float evaluate (FooBar *state)
{
    MDSPoint *curPos  = state->currentPositions;

    for (int i=0; i<numNodes; i++)
        for (int j=0; j<numNodes; j++)
        {
            float dx, dy;

            dx = curPos[i].x - curPos[j].x;
            dy = curPos[i].y - curPos[j].y;
            curDists[i][j] = sqrtf (dx * dx + dy * dy);
        }

    float c = 0;
    for (int j=0; j<numNodes; j++)
        for (int i=0; i<j; i++)
            c += trueDists[i][j];

    float sum = 0;
    for (int j=0; j<numNodes; j++)
        for (int i=0; i<j; i++)
        {
            float distDiff = trueDists[i][j] - curDists[i][j];
            sum += distDiff * distDiff / trueDists[i][j];
        }

    state->error = (1.0f / c) * sum;
    return state->error;
}

void modify (FooBar *state)
{
    MDSPoint *curPos  = state->currentPositions;

    if (reset)
    {
        reset = 0;
        for (int i=0; i<numNodes; i++)
        {
            curPos[i].x = (float) randf () * 10 - 5;
            curPos[i].y = (float) randf () * 10 - 5;
            catch_nan (curPos[i].x);
            catch_nan (curPos[i].y);
        }
    }

    memcpy (& altState, & curState, sizeof (curState));
}

float xyToTheta (float vx, float vy)
{
    if (vy > 0)
    {
        if (vx > 0)
            return atanf (vy / vx);
        else if (vx < 0)
            return M_PIF + atanf (vy / vx);
        else
            return M_PIF / 2;
    }
    else if (vy < 0)
    {
        if (vx > 0)
            return 2 * M_PIF + atanf (vy / vx);
        else if (vx < 0)
            return M_PIF + atanf (vy / vx);
        else
            return 3 * M_PIF / 2;
    }
    else
        return (vx > 0) ? 0.0f : M_PIF;
}
void iterate (FooBar *state)
{
    MDSPoint *curPos  = state->currentPositions;
    MDSPoint *nextPos = state->nextPositions;

    //for (int i=0; i<numNodes; i++)
    //{
    //    fprintf (stderr, "trueDists[%d]:", i);
    //    for (int j=0; j<numNodes; j++)
    //    {
    //        fprintf (stderr, " %f", trueDists[i][j]);
    //    }
    //    fprintf (stderr, "\n");
    //}
    for (int i=0; i<numNodes; i++)
        for (int j=0; j<numNodes; j++)
        {
            float dx, dy;

            dx = curPos[i].x - curPos[j].x;
            dy = curPos[i].y - curPos[j].y;
            curDists[i][j] = sqrtf (dx * dx + dy * dy);
        }

    float c = 0;
    for (int j=0; j<numNodes; j++)
        for (int i=0; i<j; i++)
            c += trueDists[i][j];

    for (int p=0; p<numNodes; p++)
    {
        float dEdySumpx   = 0.0;
        float dEdySumpy   = 0.0;
        float d2Edy2Sumpx = 0.0;
        float d2Edy2Sumpy = 0.0;
        for (int j=0; j<numNodes; j++)
        {
            if (j == p)
                continue;
            float dpjStar = trueDists[p][j];
            float dpj     = curDists[p][j];

            float ypx     = curPos[p].x;
            float ypy     = curPos[p].y;
            float yjx     = curPos[j].x;
            float yjy     = curPos[j].y;

            dEdySumpx += ((dpjStar - dpj) / (dpj * dpjStar)) * (ypx - yjx);
            dEdySumpy += ((dpjStar - dpj) / (dpj * dpjStar)) * (ypy - yjy);

            float factorx = (dpjStar - dpj) - (((ypx - yjx) * (ypx - yjx)) / dpj) * (1.0f + (dpjStar - dpj)/dpj);
            float factory = (dpjStar - dpj) - (((ypy - yjy) * (ypy - yjy)) / dpj) * (1.0f + (dpjStar - dpj)/dpj);
            d2Edy2Sumpx += (1.0f / dpjStar * dpj) * factorx;
            d2Edy2Sumpy += (1.0f / dpjStar * dpj) * factory;
        }

        float cinv = -2.0f / c;
        float dEdypx   = cinv * dEdySumpx;
        float dEdypy   = cinv * dEdySumpy;
        float d2Edy2px = cinv * d2Edy2Sumpx;
        float d2Edy2py = cinv * d2Edy2Sumpy;

        float dx = dEdypx / d2Edy2px;
        float dy = dEdypy / d2Edy2py;
        nextPos[p].x = curPos[p].x - magicFactor * dx;
        nextPos[p].y = curPos[p].y - magicFactor * dy;
    }

    for (int i=0; i<numNodes; i++)
    {
        curPos[i].x = nextPos[i].x;
        curPos[i].y = nextPos[i].y;
    }

    // in essence, the mds algorithm is done here.
    // However, since the algorithm makes no difference on orientation and/or mirroring,
    // Make sure the result produced doesn't fly around by defining a center point, an
    // orientation and if we should flip the result or not. Put curPos[0] in the center,
    // curPos[1] at the positive x-axis and curPos[2] at the upper half of the plane.

    // curPos[0] in center
    for (int i=0; i<numNodes; i++)
    {
        curPos[i].x -= curPos[0].x;
        curPos[i].y -= curPos[0].y;
    }

    // rotate all nodes so that curPos[1] is at x-axis
    float rotAngle = - xyToTheta (curPos[1].x, curPos[1].y);
    float cosv = cosf (rotAngle);
    float sinv = sinf (rotAngle);
    for (int i=1; i<numNodes; i++)
    {
        float rx = curPos[i].x * cosv - curPos[i].y * sinv;
        float ry = curPos[i].x * sinv + curPos[i].y * cosv;
        curPos[i].x = rx;
        curPos[i].y = ry;
    }

    // if curPos[2] is below the x-axis, flip y-coordinate
    if (curPos[2].y < 0)
    {
        for (int i=1; i<numNodes; i++)
            curPos[i].y = -curPos[i].y;
    }
}

void optimise (void)
{
    static int cnt = 0;
    if (cnt++ > 100)
    {
        memcpy (& altState, & curState, sizeof (curState));
        cnt = 0;
        MDSPoint *curPos = & altState.currentPositions[0];
        if (exEnabled)
        {
            int i = (int) (randf () * numNodes);
            int j = (int) (randf () * numNodes);
            int k = (int) (randf () * numNodes);

            print_debug ("exchanging points %d %d %d", i,j,k);
            float ix = curPos[i].x;
            float iy = curPos[i].y;
            float jx = curPos[j].x;
            float jy = curPos[j].y;
            float kx = curPos[k].x;
            float ky = curPos[k].y;

            curPos[i].x = jx;
            curPos[i].y = jy;
            curPos[j].x = kx;
            curPos[j].y = ky;
            curPos[k].x = ix;
            curPos[k].y = iy;

            //for (int i=0; i<numNodes; i++)
            //{
            //    curPos[i].x += randf () * 30 - 15;
            //    curPos[i].y += randf () * 30 - 15;
            //}
        }
    }

    for (int i=0; i<50; i++)
    {
        iterate (& altState);
        iterate (& curState);
    }

    evaluate (& curState);
    evaluate (& altState);

    if (altState.error < curState.error)
    {
        memcpy (& curState, & altState, sizeof (curState));
        print_debug ("found better state!");
    }
}

void init_rssi (char *filename)
{
    FILE *fs = fopen (filename, "r");
    if (!fs)
        exit_error ("can't open %s: %s", filename, strerror (errno));

    uint32_t to, from;
    float val;

    char buf[64];
    while (buf == fgets (buf, sizeof (buf), fs))
    {
        if (sscanf (buf, "%x %x %f", &to, &from, &val) != 3)
        {
            if (to != from)
                print_error ("sscanf failed on '%s'", buf);
            continue;
        }
        int toIdx   = get_index_by_id (to, 0);
        int fromIdx = get_index_by_id (from, 0);
        if (toIdx < 0 || fromIdx < 0)
        {
            print_warning ("skipping %x <- %x", toIdx, fromIdx);
            continue;
        }
        //print_debug ("%d <- %d %f", toIdx, fromIdx, val);
        trueDists[toIdx][fromIdx] = get_dist (toIdx, fromIdx, val);
    }
}

int main (int argc, char **argv)
{
    if (argc != 3)
        exit_error ("usage: %s <modelFile> <dataFile>", argv[0]);
    char *modelFile = argv[1];
    char *dataFile  = argv[2];

    init_model (modelFile);
    init_rssi (dataFile);

    randlib_init (0);
    executable = argv[0];
    sranddev ();

    modify (& curState);
    memcpy (& altState, & curState, sizeof (curState));
    optimise ();
    float bestError = FLT_MAX;
    double tsp = get_time ();
    double lastImprovementTsp = tsp;
    double lastPrintTsp = tsp;
    while (tsp - lastImprovementTsp < 1.4)
    {
        tsp = get_time ();
        usleep (1000);

        optimise ();

        if (bestError > curState.error)
        {
            bestError = curState.error;
            lastImprovementTsp = tsp;
            if (tsp - lastPrintTsp > 1.0)
            {
                print_debug ("error: %f", curState.error);
                lastPrintTsp = tsp;
            }
        }
    }

    MDSPoint *curPos  = curState.currentPositions;
    for (int i=0; i<numNodes; i++)
    {
        printf ("%04x %f %f\n", ids[i], curPos[i].x, curPos[i].y);
        print_debug ("%04x %f %f", ids[i], curPos[i].x, curPos[i].y);
    }

    exit(0);
}
