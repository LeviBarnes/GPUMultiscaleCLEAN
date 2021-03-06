/// @copyright (c) 2011 CSIRO
/// Australia Telescope National Facility (ATNF)
/// Commonwealth Scientific and Industrial Research Organisation (CSIRO)
/// PO Box 76, Epping NSW 1710, Australia
/// atnf-enquiries@csiro.au
///
/// The ASKAP software distribution is free software: you can redistribute it
/// and/or modify it under the terms of the GNU General Public License as
/// published by the Free Software Foundation; either version 2 of the License,
/// or (at your option) any later version.
///
/// This program is distributed in the hope that it will be useful,
/// but WITHOUT ANY WARRANTY; without even the implied warranty of
/// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
/// GNU General Public License for more details.
///
/// You should have received a copy of the GNU General Public License
/// along with this program; if not, write to the Free Software
/// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
///
/// @author Ben Humphreys <ben.humphreys@csiro.au>

// Include own header file first
#include "MultiScaleGolden.h"

// System includes
#include <vector>
#include <iostream>
#include <cstddef>
#include <cmath>

// Local includes
#include "Parameters.h"

using namespace std;

MultiScaleGolden::MultiScaleGolden(size_t n_scale_in) : n_scale(n_scale_in) {};

void MultiScaleGolden::deconvolve(const vector<float>& dirty,
                              const size_t dirtyWidth,
                              const vector<float>* psf,
                              const size_t psfWidth,
                              const vector<float>* cross,
                              const size_t crossWidth,
                              vector<float>& model,
                              vector<float>* residual)
{
    for (size_t s=0;s<n_scale;s++) residual[s] = dirty;

    // Find the peak of the PSF
    float *psfPeakVal = new float[n_scale];
    size_t *psfPeakPos = new size_t[n_scale];
    for (size_t s=0;s<n_scale;s++)
    {
       //TODO multiply by scale-dependent scale factor
       findPeak(psf[s], psfPeakVal[s], psfPeakPos[s]);
       cout << "Found peak of PSF: " << "Maximum = " << psfPeakVal[s]
         << " at location " << idxToPos(psfPeakPos[s], psfWidth).x << ","
         << idxToPos(psfPeakPos[s], psfWidth).y << " for scale " <<
         s << endl;
    }

    for (unsigned int i = 0; i < g_niters; ++i) {
        // Find the peak in the residual image
        float absPeakVal = 0.0;
        size_t absPeakPos = 0;
        float thisPeakVal = 0.0;
        size_t thisPeakPos = 0;
        int absPeakScale = 0;
        for (size_t s=0; s<n_scale; s++)
        {
           findPeak(residual[s], thisPeakVal, thisPeakPos);
           //cout << "Iteration: " << i + 1 << " - Maximum = " << absPeakVal
           //    << " at location " << idxToPos(absPeakPos, dirtyWidth).x << ","
           //    << idxToPos(absPeakPos, dirtyWidth).y << endl;

           // Check if threshold has been reached
           if (thisPeakVal > absPeakVal) {
              absPeakVal = thisPeakVal;
              absPeakPos = thisPeakPos;
              absPeakScale = s;
           }
           if (abs(absPeakVal) < g_threshold) {
               cout << "Reached stopping threshold" << endl;
               break;
           }
         }

        // Add to model
        //TODO Build the model with multiple components
        subtractPSF(psf[absPeakScale], psfWidth, model, dirtyWidth, absPeakPos,
                    psfPeakPos[absPeakScale], -absPeakVal, g_gain);

        // Subtract the PSF from the residual image
        for (size_t s=0;s<n_scale;s++) {
           subtractPSF(cross[absPeakScale*n_scale+s], crossWidth, residual[s], dirtyWidth, absPeakPos, psfPeakPos[s], absPeakVal, g_gain);
        }
    }
}

//TODO One common subtractPSF, findPeak, etc.
void MultiScaleGolden::subtractPSF(const vector<float>& psf,
                               const size_t psfWidth,
                               vector<float>& residual,
                               const size_t residualWidth,
                               const size_t peakPos, const size_t psfPeakPos,
                               const float absPeakVal,
                               const float gain)
{
    // The x,y coordinate of the peak in the residual image
    const int rx = idxToPos(peakPos, residualWidth).x;
    const int ry = idxToPos(peakPos, residualWidth).y;

    // The x,y coordinate for the peak of the PSF (usually the centre)
    const int px = idxToPos(psfPeakPos, psfWidth).x;
    const int py = idxToPos(psfPeakPos, psfWidth).y;

    // The PSF needs to be overlayed on the residual image at the position
    // where the peaks align. This is the offset between the above two points
    const int diffx = rx - px;
    const int diffy = ry - py;

    // The top-left-corner of the region of the residual to subtract from.
    // This will either be the top right corner of the PSF too, or on an edge
    // in the case the PSF spills outside of the residual image
    const int startx = max(0, rx - px);
    const int starty = max(0, ry - py);

    // This is the bottom-right corner of the region of the residual to
    // subtract from.
    const int stopx = min(residualWidth - 1, rx + (psfWidth - px - 1));
    const int stopy = min(residualWidth - 1, ry + (psfWidth - py - 1));

    for (int y = starty; y <= stopy; ++y) {
        for (int x = startx; x <= stopx; ++x) {
            residual[posToIdx(residualWidth, Position(x, y))] -= gain * absPeakVal
                    * psf[posToIdx(psfWidth, Position(x - diffx, y - diffy))];
        }
    }
}

void MultiScaleGolden::findPeak(const vector<float>& image,
                            float& maxVal, size_t& maxPos)
{
    maxVal = 0.0;
    maxPos = 0;
    const size_t size = image.size();

    for (size_t i = 0; i < size; ++i) {
        if (abs(image[i]) > abs(maxVal)) {
            maxVal = image[i];
            maxPos = i;
        }
    }
}

MultiScaleGolden::Position MultiScaleGolden::idxToPos(const int idx, const size_t width)
{
    const int y = idx / width;
    const int x = idx % width;
    return Position(x, y);
}

size_t MultiScaleGolden::posToIdx(const size_t width, const MultiScaleGolden::Position& pos)
{
    return (pos.y * width) + pos.x;
}
