#include "RecoBTag/SecondaryVertex/interface/CombinedSVComputer.h"

using namespace reco;


#define range_for(i, x) \
	for(int i = (x).begin; i != (x).end; i += (x).increment)

static edm::ParameterSet dropDeltaR(const edm::ParameterSet &pset)
{
	edm::ParameterSet psetCopy(pset);
	psetCopy.addParameter<double>("jetDeltaRMax", 99999.0);
	return psetCopy;
}

CombinedSVComputer::CombinedSVComputer(const edm::ParameterSet &params) :
	trackFlip(params.getParameter<bool>("trackFlip")),
	vertexFlip(params.getParameter<bool>("vertexFlip")),
	charmCut(params.getParameter<double>("charmCut")),
	sortCriterium(TrackSorting::getCriterium(params.getParameter<std::string>("trackSort"))),
	trackSelector(params.getParameter<edm::ParameterSet>("trackSelection")),
	trackNoDeltaRSelector(dropDeltaR(params.getParameter<edm::ParameterSet>("trackSelection"))),
	trackPseudoSelector(params.getParameter<edm::ParameterSet>("trackPseudoSelection")),
	pseudoMultiplicityMin(params.getParameter<unsigned int>("pseudoMultiplicityMin")),
	trackMultiplicityMin(params.getParameter<unsigned int>("trackMultiplicityMin")),
	minTrackWeight(params.getParameter<double>("minimumTrackWeight")),
	useTrackWeights(params.getParameter<bool>("useTrackWeights")),
	vertexMassCorrection(params.getParameter<bool>("correctVertexMass")),
	pseudoVertexV0Filter(params.getParameter<edm::ParameterSet>("pseudoVertexV0Filter")),
	trackPairV0Filter(params.getParameter<edm::ParameterSet>("trackPairV0Filter"))
{
}

inline double CombinedSVComputer::flipValue(double value, bool vertex) const
{
	return (vertex ? vertexFlip : trackFlip) ? -value : value;
}

inline CombinedSVComputer::IterationRange CombinedSVComputer::flipIterate(
						int size, bool vertex) const
{
	IterationRange range;
	if (vertex ? vertexFlip : trackFlip) {
		range.begin = size - 1;
		range.end = -1;
		range.increment = -1;
	} else {
		range.begin = 0;
		range.end = size;
		range.increment = +1;
	}

	return range;
}

const btag::TrackIPData &
CombinedSVComputer::threshTrack(const CandIPTagInfo &trackIPTagInfo,
                                const btag::SortCriteria sort,
                                const reco::Jet &jet,
                                const GlobalPoint &pv) const
{
        const CandIPTagInfo::input_container &tracks =
                                        trackIPTagInfo.selectedTracks();
        const std::vector<btag::TrackIPData> &ipData =
                                        trackIPTagInfo.impactParameterData();
        std::vector<std::size_t> indices = trackIPTagInfo.sortedIndexes(sort);

        IterationRange range = flipIterate(indices.size(), false);
        TrackKinematics kin;
        range_for(i, range) {
                std::size_t idx = indices[i];
                const btag::TrackIPData &data = ipData[idx];
                const Track &track = *tracks[idx]->bestTrack();

                if (!trackNoDeltaRSelector(track, data, jet, pv))
                        continue;

                kin.add(track);
                if (kin.vectorSum().M() > charmCut)
                        return data;
        }

        static const btag::TrackIPData dummy = {
                GlobalPoint(),
                GlobalPoint(),
                Measurement1D(-1.0, 1.0),
                Measurement1D(-1.0, 1.0),
                Measurement1D(-1.0, 1.0),
                Measurement1D(-1.0, 1.0),
                0.
        };
        return dummy;
}

const btag::TrackIPData &
CombinedSVComputer::threshTrack(const TrackIPTagInfo &trackIPTagInfo,
                                const btag::SortCriteria sort,
                                const reco::Jet &jet,
                                const GlobalPoint &pv) const
{
	const edm::RefVector<TrackCollection> &tracks =
					trackIPTagInfo.selectedTracks();
	const std::vector<btag::TrackIPData> &ipData =
					trackIPTagInfo.impactParameterData();
	std::vector<std::size_t> indices = trackIPTagInfo.sortedIndexes(sort);

	IterationRange range = flipIterate(indices.size(), false);
	TrackKinematics kin;
	range_for(i, range) {
		std::size_t idx = indices[i];
		const btag::TrackIPData &data = ipData[idx];
		const Track &track = *tracks[idx];

		if (!trackNoDeltaRSelector(track, data, jet, pv))
			continue;

		kin.add(track);
		if (kin.vectorSum().M() > charmCut) 
			return data;
	}

	static const btag::TrackIPData dummy = {
 		GlobalPoint(),
		GlobalPoint(),
		Measurement1D(-1.0, 1.0),
		Measurement1D(-1.0, 1.0),
		Measurement1D(-1.0, 1.0),
		Measurement1D(-1.0, 1.0),
		0.
	};
	return dummy;
}


TaggingVariableList
CombinedSVComputer::operator () (const TrackIPTagInfo &ipInfo,
                                 const SecondaryVertexTagInfo &svInfo) const
{
	using namespace ROOT::Math;

	edm::RefToBase<Jet> jet = ipInfo.jet();
	math::XYZVector jetDir = jet->momentum().Unit();
	TaggingVariableList vars;

        TrackKinematics vertexKinematics;
	
	// the following is specific depending on the type of vertex
        int vtx = -1;
        unsigned int numberofvertextracks = 0;

	IterationRange range = flipIterate(svInfo.nVertices(), true);
	range_for(i, range) {

		numberofvertextracks = numberofvertextracks + (svInfo.secondaryVertex(i)).nTracks();

		const Vertex &vertex = svInfo.secondaryVertex(i);
		bool hasRefittedTracks = vertex.hasRefittedTracks();
		TrackRefVector tracks = svInfo.vertexTracks(i);
		for(TrackRefVector::const_iterator track = tracks.begin(); track != tracks.end(); ++track) {
			double w = svInfo.trackWeight(i, *track);
			if (w < minTrackWeight)
				continue;
			if (hasRefittedTracks) {
				const Track actualTrack = vertex.refittedTrack(*track);
				vertexKinematics.add(actualTrack, w);
				vars.insert(btau::trackEtaRel, reco::btau::etaRel(jetDir,actualTrack.momentum()), true);
			} else {
				vertexKinematics.add(**track, w);
				vars.insert(btau::trackEtaRel, reco::btau::etaRel(jetDir,(*track)->momentum()), true);
			}
		}
		
		if (vtx < 0) vtx = i;
        }
	if(vtx>=0){
		vars.insert(btau::vertexNTracks, numberofvertextracks, true);
	}

	// after we collected vertex information we let the common code complete the job
	fillCommonVariables(vars,vertexKinematics,ipInfo,svInfo);

	vars.finalize();
	return vars;
}

TaggingVariableList
CombinedSVComputer::operator () (const CandIPTagInfo &ipInfo,
                                 const CandSecondaryVertexTagInfo &svInfo) const
{
        using namespace ROOT::Math;

        edm::RefToBase<Jet> jet = ipInfo.jet();
        math::XYZVector jetDir = jet->momentum().Unit();
        TaggingVariableList vars;

        TrackKinematics vertexKinematics;
	
	// the following is specific depending on the type of vertex
        int vtx = -1;
        unsigned int numberofvertextracks = 0;

	IterationRange range = flipIterate(svInfo.nVertices(), true);
	range_for(i, range) {

		numberofvertextracks = numberofvertextracks + (svInfo.secondaryVertex(i)).numberOfSourceCandidatePtrs();

		std::vector<CandidatePtr> tracks = svInfo.vertexTracks(i);
		for(std::vector<CandidatePtr>::const_iterator track = tracks.begin(); track != tracks.end(); ++track) {
			vertexKinematics.add(*(*track)->bestTrack(), 1.0);
			vars.insert(btau::trackEtaRel, reco::btau::etaRel(jetDir,(*track)->momentum()), true);
		}
		
		if (vtx < 0) vtx = i;
	}
	if(vtx>0){
		vars.insert(btau::vertexNTracks, numberofvertextracks, true);
	}
	
	// after we collected vertex information we let the common code complete the job
	fillCommonVariables(vars,vertexKinematics,ipInfo,svInfo);
	
	vars.finalize();
	return vars;
}

