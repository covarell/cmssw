import FWCore.ParameterSet.Config as cms

pfAllNeutralHadrons = cms.EDFilter("PdgIdPFCandidateSelector",
    src = cms.InputTag("pfNoPileUpIso"),
    pdgId = cms.vint32(111,130,310,2112)
)



