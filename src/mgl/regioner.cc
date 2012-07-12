/**
   MiracleGrue - Model Generator for toolpathing. <http://www.grue.makerbot.com>
   Copyright (C) 2011 Far McKon <Far@makerbot.com>, Hugo Boyer (hugo@makerbot.com)

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU Affero General Public License as
   published by the Free Software Foundation, either version 3 of the
   License, or (at your option) any later version.

 **/

#include <list>
#include <vector>

#include "regioner.h"

using namespace mgl;
using namespace std;

Regioner::Regioner(const SlicerConfig &slicerCfg, ProgressBar *progress)
: Progressive(progress), slicerCfg(slicerCfg) {
	// move that to its own config element?
	roofLengthCutOff = 0.5 * slicerCfg.layerH;

}

void Regioner::generateSkeleton(const LayerLoops& layerloops, 
		RegionList& regionlist) {
	int sliceCount = initRegionList(layerloops, regionlist);

	//this is to facilitate eventually operating on part of the layer list
	RegionList::iterator firstModelRegion = regionlist.begin();

	initProgress("insets", sliceCount);
	insets(layerloops.begin(), layerloops.end(),
		   firstModelRegion, regionlist.end());

	initProgress("flat surfaces", sliceCount);
	flatSurfaces(firstModelRegion, regionlist.end(), layerloops.grid);

	initProgress("roofing", sliceCount);
	roofing(firstModelRegion, regionlist.end(), layerloops.grid);

	initProgress("flooring", sliceCount);
	flooring(firstModelRegion, regionlist.end(), layerloops.grid);

	initProgress("infills", sliceCount);
	infills(firstModelRegion, regionlist.end(),	layerloops.grid);
}

size_t Regioner::initRegionList(const LayerLoops& layerloops,
							  RegionList &regionlist) {
	//TODO: take into account raft layers in the number of regions
	regionlist.resize(layerloops.size());
	return regionlist.size();
}


void Regioner::insetsForSlice(const LoopList& sliceOutlines,
		std::list<LoopList>& sliceInsets,
		const char* scadFile) {

	libthing::SegmentTable sliceOutlinesOld;
	libthing::Insets sliceInsetsOld;
	
	/*
	 This function makes use of inshelligence, which indirectly makes use of 
	 clipper.cc, a machine translated Delphi library designed to make use of 
	 SegmentTables. As it is currently not practical to quickly convert this 
	 library to use the new Loop type, we instead elected to simply convert 
	 the resulting SegmentTables into Loops
	 */

	//convert outline Loops into equivalent SegmentTables
	for (LoopList::const_iterator iter = sliceOutlines.begin();
			iter != sliceOutlines.end();
			++iter) {
		sliceOutlinesOld.push_back(std::vector<libthing::LineSegment2 > ());
		const Loop& currentLoop = *iter;
		//Convert current loop to a vector of segments
		for (Loop::const_finite_cw_iterator loopiter =
				currentLoop.clockwiseFinite();
				loopiter != currentLoop.clockwiseEnd();
				++loopiter) {
			sliceOutlinesOld.back().push_back(currentLoop.segmentAfterPoint(loopiter));
		}
	}

	//call the function with the equivalent SegmentTables
	bool writeDebugScadFiles = false;
	inshelligence(sliceOutlinesOld,
			slicerCfg.nbOfShells,
			slicerCfg.layerW,
			slicerCfg.insetDistanceMultiplier,
			scadFile,
			writeDebugScadFiles,
			sliceInsetsOld);

	//Recover loops from the resulting SegmentTable
	for (libthing::Insets::const_iterator iter = sliceInsetsOld.begin();
			iter != sliceInsetsOld.end();
			++iter) {
		sliceInsets.push_back(LoopList());
		LoopList& currentLoopList = sliceInsets.back();
		//Convert each group of insets into a list of Loops
		for (libthing::SegmentTable::const_iterator setIter = iter->begin();
				setIter != iter->end();
				++setIter) {
			currentLoopList.push_back(Loop());
			const std::vector<libthing::LineSegment2>& currentPoly = *setIter;
			Loop& currentLoop = currentLoopList.back();
			Loop::cw_iterator loopIter = currentLoop.clockwiseEnd();
			//Convert each individual inset to a Loop
			//Insert points 1 - N
			for (std::vector<libthing::LineSegment2>::const_iterator polyIter =
					currentPoly.begin();
					polyIter != currentPoly.end();
					++polyIter) {
				loopIter = currentLoop.insertPoint(polyIter->b, loopIter);
			}
			//Insert point 0
			if (!currentPoly.empty())
				loopIter = currentLoop.insertPoint(currentPoly.begin()->a, loopIter);
		}
	}
}

//void Regioner::insets(const std::vector<libthing::SegmentTable> & outlinesSegments,
//		std::vector<libthing::Insets> & insets) {
//
//	unsigned int sliceCount = outlinesSegments.size();
//	initProgress("insets", sliceCount);
//	insets.resize(sliceCount);
//
//	// slice id must be adjusted for
//	for (size_t i = 0; i < sliceCount; i++) {
//		tick();
//		const libthing::SegmentTable & sliceOutlines = outlinesSegments[i];
//		libthing::Insets & sliceInsets = insets[i];
//
//		insetsForSlice(sliceOutlines, sliceInsets);
//	}
//}

void Regioner::insets(const LayerLoops::const_layer_iterator outlinesBegin,
					  const LayerLoops::const_layer_iterator outlinesEnd,
					  RegionList::iterator regionsBegin,
					  RegionList::iterator regionsEnd) {

	LayerLoops::const_layer_iterator outline = outlinesBegin;
	RegionList::iterator region = regionsBegin;
	while(outline != outlinesBegin && region != regionsBegin) {
		tick();
		const LoopList& currentOutlines = outline->readLoops();

		insetsForSlice(currentOutlines, region->insetLoops, NULL);

		++outline;
		++region;
	}
}

void Regioner::flatSurfaces(RegionList::iterator regionsBegin,
							RegionList::iterator regionsEnd,
							const Grid& grid) {
	for (RegionList::iterator regions = regionsBegin;
		 regions != regionsEnd; regions++) {
		tick();
		const std::list<LoopList>& currentInsets = regions->insetLoops;
		GridRanges currentSurface;
		gridRangesForSlice(currentInsets, grid, regions->flatSurface);
	}
}

void Regioner::floorForSlice(const GridRanges & currentSurface,
		const GridRanges & surfaceBelow,
		const Grid & grid,
		GridRanges & flooring) {
	GridRanges floor;
	grid.gridRangeDifference(currentSurface, surfaceBelow, floor);
	grid.trimGridRange(floor, roofLengthCutOff, flooring);
}

void Regioner::roofForSlice(const GridRanges & currentSurface, const GridRanges & surfaceAbove, const Grid & grid, GridRanges & roofing) {
	GridRanges roof;
	grid.gridRangeDifference(currentSurface, surfaceAbove, roof);
	grid.trimGridRange(roof, this->roofLengthCutOff, roofing);
}

void Regioner::roofing(RegionList::iterator regionsBegin,
					   RegionList::iterator regionsEnd,
					   const Grid& grid) {

	RegionList::iterator current = regionsBegin;
	RegionList::iterator above = current;
	++above;

	while (above != regionsEnd) {
		tick();
		const GridRanges & currentSurface = current->flatSurface;
		const GridRanges & surfaceAbove = above->flatSurface;
		GridRanges & roofing = current->roofing;

		GridRanges roof;
		roofForSlice(currentSurface, surfaceAbove, grid, roof);

		grid.trimGridRange(roof, this->roofLengthCutOff, roofing);

		++current;
		++above;
	}

	tick();
	current->roofing = current->flatSurface;
}

void Regioner::flooring(RegionList::iterator regionsBegin,
						RegionList::iterator regionsEnd,
						const Grid &grid) {
	RegionList::iterator below = regionsBegin;
	RegionList::iterator current = below;
	current++;

	while (current != regionsEnd) {
		tick();
		const GridRanges & currentSurface = current->flatSurface;
		const GridRanges & surfaceBelow = below->flatSurface;
		GridRanges & flooring = current->flooring;

		floorForSlice(currentSurface, surfaceBelow, grid, flooring);
	}

	tick();
	regionsBegin->flooring = regionsBegin->flatSurface;

}

void Regioner::infills(RegionList::iterator regionsBegin,
					   RegionList::iterator regionsEnd,
					   const Grid &grid) {
	
	for (RegionList::iterator current = regionsBegin;
		 current != regionsEnd; current++) {

		const GridRanges &surface = current->flatSurface;
		tick();

		// Solids
		GridRanges combinedSolid;

		combinedSolid.xRays.resize(surface.xRays.size());
		combinedSolid.yRays.resize(surface.yRays.size());

		//TODO: no reason to get bounds separately from the combination

		//find the bounds we will be combinging regions across
		RegionList::iterator firstFloor = current;
		for (int i = 0; i < slicerCfg.floorLayerCount &&
   				        firstFloor != regionsBegin; i++)
			--firstFloor;

		RegionList::iterator lastRoof = current;
		for (int i = 0; i < slicerCfg.roofLayerCount &&
				        lastRoof != regionsEnd; i++)
			++lastRoof;

		//combine floors
		for (RegionList::iterator floor = firstFloor;
			 floor <= current; ++floor) {
			GridRanges multiFloor;

			grid.gridRangeUnion(combinedSolid, floor->flooring, multiFloor);
			combinedSolid = multiFloor;
		}

		//combine roofs
		for (RegionList::iterator roof = current;
			 roof <= lastRoof; ++roof) {
			GridRanges multiRoof;

			grid.gridRangeUnion(combinedSolid, roof->roofing, multiRoof);
			combinedSolid = multiRoof;
		}

		// solid now contains the combination of combinedSolid regions from
		// multiple slices. We need to extract the perimeter from it

		grid.gridRangeIntersection(surface, combinedSolid, current->solid);

		// TODO: move me to the slicer
		GridRanges sparseInfill;
		size_t infillSkipCount = (int) (1 / slicerCfg.infillDensity) - 1;
		grid.subSample(surface, infillSkipCount, sparseInfill);

		grid.gridRangeUnion(current->solid, sparseInfill, current->infill);

		//TODO: this doesn't seem right, but its what it was doing before I
		//converted to iterators
		current->sparse = current->infill;

	}

}

void Regioner::gridRangesForSlice(const std::list<LoopList>& allInsetsForSlice,
		const Grid& grid,
		GridRanges & surface) {
	const LoopList& innerMostLoops = allInsetsForSlice.back();
	grid.createGridRanges(innerMostLoops, surface);
}




