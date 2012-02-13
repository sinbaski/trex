function [spec, errors, LLF, residuals, sigmas, summary] = xxl_fit(coeff, returns, prefitted)

fprintf(2, '[%s]\n', datestr(now, 'HH:MM:SS'));
if prefitted
    [spec, errors, LLF, residuals, sigmas, summary] = ...
        garchfit(coeff, returns);
    if ~isempty(strfind(summary.converge, 'converged'))
        fprintf(2, 'The prefitted model still fits.\n');
        return;
    else
        fprintf(2, 'The prefitted model no longer fits.\n');
    end
end

modelFitted = 0;
residuals = NaN(size(returns, 1), 1);
sigmas = NaN(size(returns, 1), 1);

% find the simpliest fitting model.
for a = 4:10
    for b = a:10
        for c = 1:3
            if modelFitted
                break;
            end
            spec = garchset('Distribution' , 'T',...
                            'Display', 'off', ...
                            'VarianceModel', 'GJR',...
                            'P', c, 'Q', c, ...
                            'R', b, 'M', a);
            [spec, errors, LLF, residuals, sigmas, summary] = ...
                garchfit(spec, returns);
            fprintf(2, '%u %u %u %u ', a, b, c, c);
            if ~isempty(strfind(summary.converge, 'converged'))
                modelFitted = 1;
                fprintf(2, 'converges.\n');
            else
                fprintf(2, 'does not converge.\n');
            end
        end
    end
end
if ~modelFitted
    return;
end

fprintf(2, 'Now the lratio test will be run.\n');
% do the lratiotest
names = ['R', 'M', 'P', 'Q'];
a = 1;
while a <= length(names)
    spec2 = garchset('Distribution' , 'T'  , 'Display', 'off', ...
                    'VarianceModel', 'GJR', 'P', spec.P, 'Q', spec.Q, ...
                    'R', spec.R, 'M', spec.M);
    x = garchget(spec, names(a));
    spec2 = garchset(spec2, names(a), x + 1);
    [spec2, errors2, LLF2, residuals2, sigmas2, summary2] = ...
        garchfit(spec2, returns);
    if isempty(strfind(summary2.converge, 'converged'))
        fprintf(2, 'spec with %s = %d does not converge.\n',...
                names(a), x + 1);
        a = a + 1;
    else
        [h,pValue,stat,cValue] = lratiotest(LLF2, LLF, 1);
        if h == 1
            spec = spec2;
            errors = errors2;
            LLF = LLF2;
            residuals = residuals2;
            sigmas = sigmas2;
            summary = summary2;
            fprintf(2, '%d %d %d %d\n', spec.R, spec.M, spec.P, spec.Q);
        else
            fprintf(2, 'lratio test has been run for %s.\n', ...
                    names(a));
            a = a + 1;
        end
    end
end
