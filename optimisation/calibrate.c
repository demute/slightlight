#include "common.h"
#include "randlib.h"

#define MAX_NODES 20
float R[MAX_NODES] = {0};
float T[MAX_NODES] = {0};
float K = 0;
float bestR[MAX_NODES] = {0};
float bestT[MAX_NODES] = {0};
float bestK = 1.0;
float calRssi[MAX_NODES][MAX_NODES] = {{0}};
float trueDists[MAX_NODES][MAX_NODES] = {{0}};
float compDists[MAX_NODES][MAX_NODES] = {{0}};
int ids[MAX_NODES] = {0};
int numNodes = 0;
float a = 1.0;

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
    exit_error ("failed");
}

float get_dist (int i, int j, float rssi)
{
    float dij = __exp10f ((rssi - R[i] - T[j]) / K);
    return dij;
}

void compute_dists (void)
{
    for (int i=0; i<numNodes; i++)
        for (int j=0; j<numNodes; j++)
            if (i != j)
            {
                float dij = __exp10f ((calRssi[i][j] - R[i] - T[j]) / K);
                compDists[i][j] = dij;
            }
}

float compute_cost (void)
{
    compute_dists ();
    float cost = 0;
    for (int i=0; i<numNodes; i++)
        for (int j=0; j<numNodes; j++)
            if (i != j)
            {
                float d = compDists[i][j] - trueDists[i][j];
                cost += d * d / trueDists[i][j];
            }

    return cost;
}

static inline float r (float w) { return (float) runif (-w, w); }

void optimize (void)
{
    compute_dists ();
    float step = 0.0001f;
    float Kderiv = 0;
    float Rderiv[MAX_NODES] = {0};
    float Tderiv[MAX_NODES] = {0};
    float factor = -2.0f * logf (10) / K;
    for (int i=0; i<numNodes; i++)
    {
        for (int j=0; j<numNodes; j++)
        {
            if (i != j)
            {
                float dij = compDists[i][j];
                float tij = trueDists[i][j];
                float dji = compDists[j][i];
                float tji = trueDists[j][i];

                Rderiv[i] +=  dij * (dij - tij) / tij;
                Tderiv[i] +=  dji * (dji - tji) / tji;
                Kderiv    += (dij * (dij - tij) / tij) * (calRssi[i][j] - R[i] - T[j]);
            }
        }
        Rderiv[i] *= factor;
        Tderiv[i] *= factor;
    }
    Kderiv *= factor / K;

    float noise = 0.1f;
    for (int i=0; i<numNodes; i++)
    {
        R[i] -= step * Rderiv[i] * (1.0f + a * r (noise));
        T[i] -= step * Tderiv[i] * (1.0f + a * r (noise));
        //R[i] -= a * r (noise);
        //T[i] -= a * r (noise);
    }
    K -= step * Kderiv * (1.0f + a * r (noise));
    //K -= a * r (noise);
    a *= 0.99999996f;
}

int uninterrupted = 1;
void sig_handler (int sig)
{
    uninterrupted = 0;
}

void init_rssi_and_true_dists (char *infile)
{
    FILE *fs = fopen (infile, "r");
    if (!fs)
        exit_error ("can't open %s: %s", infile, strerror (errno));

    uint32_t to, from;
    float rssi;
    float dist;

    char buf[64];
    while (buf == fgets (buf, sizeof (buf), fs))
    {
        if (sscanf (buf, "%x %x %f %f", &to, &from, &rssi, &dist) != 4)
        {
            if (to != from)
                print_error ("sscanf failed on '%s'", buf);
            continue;
        }
        int toIdx   = get_index_by_id (to, 1);
        int fromIdx = get_index_by_id (from, 1);
        print_debug ("%d <- %d %f %f", toIdx, fromIdx, rssi, dist);
        if (toIdx == fromIdx)
            rssi = 0;
        calRssi[toIdx][fromIdx] = rssi;
        trueDists[toIdx][fromIdx] = dist;
    }
}

void init_k (void)
{
    float sumRssi = 0;
    float sumDist = 0;
    int cnt = 0;
    for (int i=0; i<numNodes; i++)
        for (int j=0; j<numNodes; j++)
            if (i != j)
            {
                sumRssi += calRssi[i][j];
                sumDist += trueDists[i][j];
                cnt++;
            }
    float avgRssi = sumRssi / cnt;
    float avgDist = sumDist / cnt;

    sumRssi = 0;
    sumDist = 0;

    for (int i=0; i<numNodes; i++)
        for (int j=0; j<numNodes; j++)
            if (i != j)
            {
                sumRssi += powf (calRssi[i][j] - avgRssi, 2.0f);
                sumDist += powf (trueDists[i][j] - avgDist, 2.0f);
            }

    float varRssi = sumRssi / (cnt - 1);
    float varDist = sumDist / (cnt - 1);

    float stdRssi = sqrtf (varRssi);
    float stdDist = sqrtf (varDist);

    float d0 = avgDist - stdDist;
    float d1 = avgDist + stdDist;

    float rssi0 = avgRssi + stdRssi;
    float rssi1 = avgRssi - stdRssi;

    K = (rssi0 - rssi1) / (log10f (d0) - log10f (d1));

    print_debug ("avgRssi: %f avgDist: %f varRssi: %f varDist: %f stdRssi: %f stdDist: %f => init K: %f", avgRssi, avgDist, varRssi, varDist, stdRssi, stdDist, K);
}

void init_rt (void)
{
    bzero (R, sizeof (R));
    bzero (T, sizeof (T));
    int rCnt[MAX_NODES] = {0};
    int tCnt[MAX_NODES] = {0};
    for (int i=0; i<numNodes; i++)
        for (int j=0; j<numNodes; j++)
            if (i != j)
            {
                float rtSum = calRssi[i][j] - K * log10f (trueDists[i][j]);
                print_debug ("calRssi[i][j] - K * log10f (trueDists[i][j]) = %f - %f * log10f (%f) = %f - %f * %f = %f (%x <-> %x)",
                             calRssi[i][j], K, trueDists[i][j], calRssi[i][j], K, log10f (trueDists[i][j]), rtSum, ids[i], ids[j]);
                float rContrib = 0.5f * rtSum;
                float tContrib = 0.5f * rtSum;
                rCnt[i]++;
                tCnt[j]++;
                R[i] += rContrib;
                T[j] += tContrib;
            }

    for (int i=0; i<numNodes; i++)
    {
        R[i] /= rCnt[i];
        T[i] /= tCnt[i];
        print_debug ("R[%d] = %f", i, R[i]);
        print_debug ("T[%d] = %f", i, T[i]);
    }
}

void dump_initial_dists (void)
{
    for (int i=0; i<numNodes; i++)
        for (int j=0; j<numNodes; j++)
            if (i != j)
            {
                print_debug ("test: d%d%d (%f) = %f // %x <-> %x", i, j, calRssi[i][j], get_dist (i, j, calRssi[i][j]), ids[i], ids[j]);
            }
}

int main (int argc, char **argv)
{
    randlib_init (0);

    if (argc != 3)
        exit_error ("usage: %s <infile> <outfile>", argv[0]);

    char *infile = argv[1];
    char *outfile = argv[2];

    //char *infile = "/Users/manne/field_test/data.gps_calibration";
    init_rssi_and_true_dists (infile);
    FILE *outfs = fopen (outfile, "w");
    if (!outfs)
        exit_error ("can't open file for writing: %s: %s", outfile, strerror (errno));

    init_k ();
    init_rt ();

    //dump_initial_dists ();

    float initialCost = compute_cost ();
    float cost, lastCost=0;
    (void) lastCost;
    float lowestCost = initialCost;
    memcpy (bestR, R, sizeof (R));
    memcpy (bestT, T, sizeof (T));
    bestK = K;

    cost = initialCost;
    print_debug ("cost: %f", cost);

    signal (SIGINT, sig_handler);
    int lcnt = 0;
    int cnt = 0;
    double tsp = get_time ();
    double lastImprovementTsp = tsp;
    double lastPrintTsp = tsp;

    while (uninterrupted && tsp - lastImprovementTsp < 2.0)
    {
        tsp = get_time ();
        cnt ++;
        optimize ();
        cost = compute_cost ();
        if (lowestCost > cost)
        {
            memcpy (bestR, R, sizeof (R));
            memcpy (bestT, T, sizeof (T));
            bestK = K;
            lowestCost = cost;
            lcnt = cnt;
            cnt = 0;
            lastImprovementTsp = tsp;
        }

        if (cnt - lcnt > 500)
        {
            memcpy (R, bestR, sizeof (R));
            memcpy (T, bestT, sizeof (T));
        }
        if (tsp - lastPrintTsp > 1.0)
        {
            print_debug ("lowestCost: %f cost: %f a:%f", lowestCost, cost, a);
            lastPrintTsp = tsp;
        }
    }

    print_debug ("done! configuration:");
    fprintf (outfs, "%0.6f\n", bestK);
    print_debug (   "%0.6f",   bestK);
    for (int i=0; i<numNodes; i++)
    {
        fprintf (outfs, "%04x %0.6f %0.6f\n", ids[i], bestR[i], bestT[i]);
        print_debug (   "%04x %0.6f %0.6f",   ids[i], bestR[i], bestT[i]);
    }
    fclose (outfs);

    print_debug ("initialCost:%f optimised cost:%f", initialCost, lowestCost);
    print_debug ("wrote configuration to %s", outfile);
    return 0;
}
