%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
% This code calculates the weighted generalized Hurst exponent H(q) from 
% the scaling of the renormalized q-moments of the distribution 
%
%       <|x(t+r)-x(t)|^q>_w/<x(t)^q>_w ~ r^[qH(q)]
%
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
% H = genhurst(S)
% S is 1xT data series (T>50 recommended)
% calculates H(q=1)
%
% H = genhurst(S,q)
% specifies the exponent q which can be a vector (default value q=1)
%
% H = genhurst(S,q,maxT)
% specifies value maxT of the scaling window, default value maxT=19
%
% [H,sH]=genhurst(S,...)
% estimates the standard deviation sH(q)
%
% example:
%   generalized Hurst exponent for a random gaussian process
%   H=genhurstw(cumsum(randn(10000,1)))
% or 
%   H=genhurstw(cumsum(randn(10000,1)),q) to calculate H(q) with arbitrary q
%
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
% for the generalized Hurst exponent method please refer to:
%   T. Di Matteo et al. Physica A 324 (2003) 183-188 
%   T. Di Matteo et al. Journal of Banking & Finance 29 (2005) 827-851
%   T. Di Matteo Quantitative Finance, 7 (2007) 21-36
%   R. Morales et al.Physica A, 391 (2012) 3180-3189.
%----------------------------------------------------------------------------------------- 
% Contact:
% Tomaso Aste t.aste@kent.ac.uk
%-----------------------------------------------------------------------------------------
function [mH,sH]=genhurst(S,q,maxT) %
if nargin < 2, q = 1; maxT = 19; end
if nargin < 3,  maxT = 19;  end
if size(S,1)==1 & size(S,2)>1
    S = S';
elseif size(S,1)>1 & size(S,2)>1
    fprintf('S must be 1xT  \n')
    return
end
if size(S,1) < (maxT*4 | 60)
    warning('Data serie very short!')
end
L  = length(S);
lq = length(q);
H  = [];
ttmm =[];
k = 0;
for Tmax=5:maxT
    k = k+1;
    x = 1:Tmax;
    ttmm(end+1)=Tmax;
    mcord1 = [];
    mcord2 = [];
	for tt = 1:Tmax
        dV = S(L:-tt:(tt+1))-S((L-tt):-tt:1);
        VV = S(L:-tt:1)';
        N = length(dV)+1;
        X = 1:N;
        Y = VV;
        mx = sum(X)/N;
        SSxx = sum(X.^2) - N*mx^2;
        my   = sum(Y)/N;
        SSxy = sum(X.*Y) - N*mx*my;
        cc(1) = SSxy/SSxx;
        cc(2) = my - cc(1)*mx;
        ddVd  = dV + cc(1);
        VVVd  = VV - cc(1).*[1:N] - cc(2);  
        for qq=1:lq
            mcord(tt,qq)=mean(abs(ddVd).^q(qq))/mean(abs(VVVd).^q(qq));
        end
    end
    mx = mean(log10(x));
    SSxx = sum(log10(x).^2) - Tmax*mx^2;
    for qq=1:lq
        my = mean(log10(mcord(:,qq)));
        SSxy = sum(log10(x).*log10(mcord(:,qq))') - Tmax*mx*my;
        H(k,qq) = SSxy/SSxx;  
    end
end
mH = mean(H)'./q(:);
if nargout == 2
    sH = std(H)'./q(:);
elseif nargout == 1
    sH = [];
end